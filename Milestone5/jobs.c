#define _POSIX_C_SOURCE 200809L
#include "jobs.h"
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>
#include <sys/wait.h>

static job_t jobs[MAX_JOBS];
static int next_id = 1;
static volatile sig_atomic_t sigchld_pending = 0;
static pid_t sh_pgid = -1;
static int sh_terminal = STDIN_FILENO;
static int interactive = 0;

static const char *state_name(job_state_t s) {
    return s == JOB_STOPPED ? "Stopped" : s == JOB_DONE ? "Done" : "Running";
}

static int find_by_pgid(pid_t pgid) {
    for (int i = 0; i < MAX_JOBS; ++i)
        if (jobs[i].id && jobs[i].pgid == pgid) return i;
    return -1;
}

static int find_by_id(int id) {
    for (int i = 0; i < MAX_JOBS; ++i)
        if (jobs[i].id == id) return i;
    return -1;
}

static int alloc_slot(void) {
    for (int i = 0; i < MAX_JOBS; ++i)
        if (jobs[i].id == 0) return i;
    return -1;
}

static int next_job_id(void) {
    for (int n = 0; n < MAX_JOBS * 2; ++n) {
        int candidate = next_id++;
        if (next_id > 9999) next_id = 1;
        if (find_by_id(candidate) < 0) return candidate;
    }
    return -1;
}

int jobs_init(void) {
    interactive = isatty(sh_terminal);
    if (!interactive) {
        sh_pgid = getpgrp();
        return 0;
    }

    while (tcgetpgrp(sh_terminal) != (sh_pgid = getpgrp())) {
        kill(-sh_pgid, SIGTTIN);
    }

    sh_pgid = getpid();
    if (setpgid(sh_pgid, sh_pgid) < 0 && errno != EACCES) {
        perror("setpgid");
        return -1;
    }
    if (tcsetpgrp(sh_terminal, sh_pgid) < 0) {
        perror("tcsetpgrp");
        return -1;
    }

    signal(SIGINT, SIG_IGN);
    signal(SIGQUIT, SIG_IGN);
    signal(SIGTSTP, SIG_IGN);
    signal(SIGTTIN, SIG_IGN);
    signal(SIGTTOU, SIG_IGN);

    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = jobs_sigchld_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART | SA_NOCLDSTOP;
    /* SA_NOCLDSTOP is intentionally removed below so stopped jobs are visible. */
    sa.sa_flags = SA_RESTART;
    if (sigaction(SIGCHLD, &sa, NULL) < 0) {
        perror("sigaction");
        return -1;
    }
    return 0;
}

void jobs_shutdown(void) {
    for (int i = 0; i < MAX_JOBS; ++i) {
        free(jobs[i].command);
        jobs[i].command = NULL;
        jobs[i].id = 0;
    }
}

void jobs_sigchld_handler(int signo) {
    (void)signo;
    sigchld_pending = 1;
}

static void handle_status(pid_t pid, int status) {
    if (WIFSTOPPED(status) || WIFCONTINUED(status)) {
        pid_t pgid = getpgid(pid);
        if (pgid < 0) return;
        int idx = find_by_pgid(pgid);
        if (idx < 0) return;
        jobs[idx].state = WIFSTOPPED(status) ? JOB_STOPPED : JOB_RUNNING;
        return;
    }

    if (WIFEXITED(status) || WIFSIGNALED(status)) {
        /* The reaped PID may no longer have a process-group entry. Find the
           tracked group that has disappeared completely. */
        for (int i = 0; i < MAX_JOBS; ++i) {
            if (!jobs[i].id) continue;
            if (kill(-jobs[i].pgid, 0) < 0 && errno == ESRCH)
                jobs[i].state = JOB_DONE;
        }
    }
}

void jobs_reap(void) {
    int status;
    pid_t pid;
    sigchld_pending = 0;
    for (;;) {
        pid = waitpid(-1, &status, WNOHANG | WUNTRACED | WCONTINUED);
        if (pid <= 0) break;
        handle_status(pid, status);
    }

    for (int i = 0; i < MAX_JOBS; ++i) {
        if (!jobs[i].id || jobs[i].state != JOB_DONE) continue;
        printf("[%d] Done %s\n", jobs[i].id, jobs[i].command);
        free(jobs[i].command);
        jobs[i].command = NULL;
        jobs[i].id = 0;
    }
}

int jobs_add(pid_t pgid, job_state_t state, const char *command) {
    int slot = alloc_slot();
    int id = next_job_id();
    if (slot < 0 || id < 0) {
        fprintf(stderr, "shellforge: job table full\n");
        return -1;
    }
    jobs[slot].id = id;
    jobs[slot].pgid = pgid;
    jobs[slot].state = state;
    jobs[slot].command = strdup(command ? command : "");
    if (!jobs[slot].command) {
        jobs[slot].id = 0;
        perror("strdup");
        return -1;
    }
    return id;
}

int jobs_update_pgid(pid_t pgid, job_state_t state) {
    int idx = find_by_pgid(pgid);
    if (idx < 0) return -1;
    jobs[idx].state = state;
    return jobs[idx].id;
}

int jobs_remove(int id) {
    int idx = find_by_id(id);
    if (idx < 0) return -1;
    free(jobs[idx].command);
    memset(&jobs[idx], 0, sizeof(jobs[idx]));
    return 0;
}

void jobs_print(void) {
    jobs_reap();
    for (int i = 0; i < MAX_JOBS; ++i) {
        if (jobs[i].id)
            printf("[%d] %-7s %s\n", jobs[i].id, state_name(jobs[i].state), jobs[i].command);
    }
}

job_t *jobs_get(int id) {
    int idx = find_by_id(id);
    return idx < 0 ? NULL : &jobs[idx];
}

job_t *jobs_current(void) {
    job_t *best = NULL;
    for (int i = 0; i < MAX_JOBS; ++i) {
        if (!jobs[i].id || jobs[i].state == JOB_DONE) continue;
        if (!best || jobs[i].id > best->id) best = &jobs[i];
    }
    return best;
}

job_t *jobs_previous(void) {
    job_t *best = NULL, *second = NULL;
    for (int i = 0; i < MAX_JOBS; ++i) {
        if (!jobs[i].id || jobs[i].state == JOB_DONE) continue;
        if (!best || jobs[i].id > best->id) { second = best; best = &jobs[i]; }
        else if (!second || jobs[i].id > second->id) second = &jobs[i];
    }
    return second;
}

static int wait_foreground(job_t *job) {
    int status;
    int stopped = 0;
    if (interactive && tcsetpgrp(sh_terminal, job->pgid) < 0) perror("tcsetpgrp");

    for (;;) {
        pid_t r = waitpid(-job->pgid, &status, WUNTRACED);
        if (r > 0) {
            if (WIFSTOPPED(status)) { stopped = 1; break; }
            continue;
        }
        if (r < 0 && errno == EINTR) continue;
        if (r < 0 && errno == ECHILD) break;
        if (r < 0) { perror("waitpid"); break; }
    }

    if (interactive && tcsetpgrp(sh_terminal, sh_pgid) < 0) perror("tcsetpgrp");
    if (stopped) {
        job->state = JOB_STOPPED;
        printf("[%d] Stopped %s\n", job->id, job->command);
        return 148;
    }
    return 0;
}

int jobs_fg(int id) {
    job_t *job = (id > 0) ? jobs_get(id) : jobs_current();
    if (!job) { fprintf(stderr, "fg: no such job\n"); return 1; }
    if (kill(-job->pgid, SIGCONT) < 0) { perror("fg: SIGCONT"); return 1; }
    job->state = JOB_RUNNING;
    int rc = wait_foreground(job);
    if (rc == 148) return 148;
    jobs_remove(job->id);
    return 0;
}

int jobs_bg(int id) {
    job_t *job = NULL;
    if (id <= 0) job = jobs_current();
    else job = jobs_get(id);
    if (!job) { fprintf(stderr, "bg: no such job\n"); return 1; }
    if (kill(-job->pgid, SIGCONT) < 0) { perror("bg: SIGCONT"); return 1; }
    job->state = JOB_RUNNING;
    printf("[%d] %s &\n", job->id, job->command);
    return 0;
}

int jobs_mark_done(pid_t pgid) {
    int idx = find_by_pgid(pgid);
    if (idx < 0) return -1;
    jobs[idx].state = JOB_DONE;
    return 0;
}

int jobs_has_active(void) {
    for (int i = 0; i < MAX_JOBS; ++i)
        if (jobs[i].id && jobs[i].state != JOB_DONE) return 1;
    return 0;
}

pid_t shell_pgid(void) { return sh_pgid; }
int shell_terminal(void) { return sh_terminal; }
int shell_interactive(void) { return interactive; }
