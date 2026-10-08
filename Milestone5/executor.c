#define _POSIX_C_SOURCE 200809L
#include "executor.h"
#include "builtin.h"
#include "jobs.h"
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

static int redirect_fd(const command_t *c) {
    if (c->input_file) {
        int fd = open(c->input_file, O_RDONLY);
        if (fd < 0) { perror(c->input_file); return -1; }
        if (dup2(fd, STDIN_FILENO) < 0) { perror("dup2"); close(fd); return -1; }
        close(fd);
    }
    if (c->output_file) {
        int flags = O_WRONLY | O_CREAT | (c->append_output ? O_APPEND : O_TRUNC);
        int fd = open(c->output_file, flags, 0666);
        if (fd < 0) { perror(c->output_file); return -1; }
        if (dup2(fd, STDOUT_FILENO) < 0) { perror("dup2"); close(fd); return -1; }
        close(fd);
    }
    if (c->error_file) {
        int fd = open(c->error_file, O_WRONLY | O_CREAT | O_TRUNC, 0666);
        if (fd < 0) { perror(c->error_file); return -1; }
        if (dup2(fd, STDERR_FILENO) < 0) { perror("dup2"); close(fd); return -1; }
        close(fd);
    }
    if (c->stderr_to_stdout && dup2(STDOUT_FILENO, STDERR_FILENO) < 0) {
        perror("dup2"); return -1;
    }
    return 0;
}

static void restore_child_signals(void) {
    signal(SIGINT, SIG_DFL); signal(SIGQUIT, SIG_DFL); signal(SIGTSTP, SIG_DFL);
    signal(SIGTTIN, SIG_DFL); signal(SIGTTOU, SIG_DFL); signal(SIGCHLD, SIG_DFL);
}

static char *pipeline_text(const pipeline_t *p) {
    if (p->command_line) return strdup(p->command_line);
    size_t cap = 128, len = 0;
    char *s = malloc(cap);
    if (!s) return NULL;
    s[0] = '\0';
    for (size_t i = 0; i < p->count; ++i) {
        if (i && len + 3 >= cap) { cap *= 2; s = realloc(s, cap); if (!s) return NULL; }
        if (i) { strcat(s, " | "); len += 3; }
        for (size_t j = 0; j < p->commands[i].argc; ++j) {
            size_t add = strlen(p->commands[i].argv[j]) + 2;
            while (len + add + 1 >= cap) cap *= 2;
            s = realloc(s, cap); if (!s) return NULL;
            if (j) { strcat(s, " "); ++len; }
            strcat(s, p->commands[i].argv[j]); len += strlen(p->commands[i].argv[j]);
        }
    }
    return s;
}

static int run_job_control_builtin(const command_t *c) {
    if (!strcmp(c->argv[0], "jobs")) { if (c->argc != 1) { fprintf(stderr, "jobs: no arguments expected\n"); return 1; } jobs_print(); return 0; }
    if (!strcmp(c->argv[0], "fg") || !strcmp(c->argv[0], "bg")) {
        if (c->argc > 2) { fprintf(stderr, "%s: too many arguments\n", c->argv[0]); return 1; }
        int id = 0;
        if (c->argc == 2) {
            const char *s = c->argv[1]; if (*s == '%') ++s;
            char *end = NULL; long v = strtol(s, &end, 10);
            if (*s == '\0' || *end || v <= 0) { fprintf(stderr, "%s: invalid job id\n", c->argv[0]); return 1; }
            id = (int)v;
        }
        return !strcmp(c->argv[0], "fg") ? jobs_fg(id) : jobs_bg(id);
    }
    return -1;
}

int execute_pipeline(const pipeline_t *p) {
    if (!p || p->count == 0) return 0;

    if (p->count == 1 && !p->background) {
        int jc = run_job_control_builtin(&p->commands[0]);
        if (jc >= 0) return jc;
        if (is_builtin(&p->commands[0])) {
            if (!strcmp(p->commands[0].argv[0], "exit")) return 99;
            return execute_builtin(&p->commands[0]);
        }
    }

    size_t n = p->count;
    int (*pipes)[2] = n > 1 ? calloc(n - 1, sizeof(*pipes)) : NULL;
    pid_t *pids = calloc(n, sizeof(*pids));
    if (!pids || (n > 1 && !pipes)) { perror("calloc"); free(pids); free(pipes); return 1; }
    for (size_t i = 0; i + 1 < n; ++i) {
        if (pipe(pipes[i]) < 0) { perror("pipe"); for (size_t j=0;j<i;++j){close(pipes[j][0]);close(pipes[j][1]);} free(pipes); free(pids); return 1; }
    }

    pid_t pgid = 0;
    size_t started = 0;
    for (size_t i = 0; i < n; ++i) {
        pid_t pid = fork();
        if (pid < 0) { perror("fork"); break; }
        if (pid == 0) {
            restore_child_signals();
            pid_t child_pgid = pgid ? pgid : getpid();
            if (setpgid(0, child_pgid) < 0) _exit(126);
            if (i > 0 && dup2(pipes[i-1][0], STDIN_FILENO) < 0) { perror("dup2"); _exit(126); }
            if (i + 1 < n && dup2(pipes[i][1], STDOUT_FILENO) < 0) { perror("dup2"); _exit(126); }
            for (size_t j=0;j+1<n;++j){close(pipes[j][0]);close(pipes[j][1]);}
            if (redirect_fd(&p->commands[i]) < 0) _exit(126);
            if (is_builtin(&p->commands[i])) { int rc = execute_builtin(&p->commands[i]); fflush(NULL); _exit(rc); }
            execvp(p->commands[i].argv[0], p->commands[i].argv);
            perror(p->commands[i].argv[0]); _exit(127);
        }
        if (!pgid) pgid = pid;
        if (setpgid(pid, pgid) < 0 && errno != EACCES) perror("setpgid");
        pids[started++] = pid;
    }
    for (size_t i=0;i+1<n;++i){close(pipes[i][0]);close(pipes[i][1]);}
    free(pipes); free(pids);

    if (started != n) { if (pgid) kill(-pgid, SIGTERM); return 1; }

    char *text = pipeline_text(p);
    if (p->background) {
        int id = jobs_add(pgid, JOB_RUNNING, text ? text : "background job");
        free(text);
        if (id < 0) { kill(-pgid, SIGTERM); return 1; }
        printf("[%d] %ld\n", id, (long)pgid);
        return 0;
    }

    job_t temp = { .id = 0, .pgid = pgid, .state = JOB_RUNNING, .command = text ? text : strdup("foreground job") };
    if (shell_interactive() && tcsetpgrp(shell_terminal(), pgid) < 0) perror("tcsetpgrp");
    int status, stopped = 0;
    for (;;) {
        pid_t r = waitpid(-pgid, &status, WUNTRACED);
        if (r > 0) { if (WIFSTOPPED(status)) { stopped = 1; break; } continue; }
        if (r < 0 && errno == EINTR) continue;
        if (r < 0 && errno == ECHILD) break;
        if (r < 0) { perror("waitpid"); break; }
    }
    if (shell_interactive() && tcsetpgrp(shell_terminal(), shell_pgid()) < 0) perror("tcsetpgrp");
    if (stopped) {
        int id = jobs_add(pgid, JOB_STOPPED, temp.command);
        printf("[%d] Stopped %s\n", id, temp.command);
    }
    free(temp.command);
    return stopped ? 148 : 0;
}
