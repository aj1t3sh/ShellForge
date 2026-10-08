#include "executor.h"
#include "builtin.h"
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/wait.h>
#include <errno.h>
#include <signal.h>
#include <string.h>

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

static int wait_for_all(pid_t *pids, size_t n) {
    int result = 1;
    for (size_t i = 0; i < n; ++i) {
        int status;
        while (waitpid(pids[i], &status, 0) < 0) {
            if (errno == EINTR) continue;
            perror("waitpid"); free(pids); return 1;
        }
        if (i == n - 1) {
            if (WIFEXITED(status)) result = WEXITSTATUS(status);
            else if (WIFSIGNALED(status)) result = 128 + WTERMSIG(status);
        }
    }
    free(pids);
    return result;
}

int execute_pipeline(const pipeline_t *p) {
    if (!p || p->count == 0) return 0;

    if (p->count == 1 && !p->background && is_builtin(&p->commands[0])) {
        if (strcmp(p->commands[0].argv[0], "exit") == 0) return 99;
        return execute_builtin(&p->commands[0]);
    }

    size_t n = p->count;
    int (*pipes)[2] = n > 1 ? calloc(n - 1, sizeof(*pipes)) : NULL;
    pid_t *pids = calloc(n, sizeof(*pids));
    if (!pids || (n > 1 && !pipes)) { perror("calloc"); free(pids); free(pipes); return 1; }

    for (size_t i = 0; i + 1 < n; ++i) {
        if (pipe(pipes[i]) < 0) {
            perror("pipe");
            free(pids); free(pipes);
            return 1;
        }
    }

    size_t started = 0;
    for (size_t i = 0; i < n; ++i) {
        pid_t pid = fork();
        if (pid < 0) { perror("fork"); break; }
        if (pid == 0) {
            signal(SIGINT, SIG_DFL);
            signal(SIGQUIT, SIG_DFL);
            if (i > 0 && dup2(pipes[i - 1][0], STDIN_FILENO) < 0) { perror("dup2"); _exit(126); }
            if (i + 1 < n && dup2(pipes[i][1], STDOUT_FILENO) < 0) { perror("dup2"); _exit(126); }
            for (size_t j = 0; j + 1 < n; ++j) {
                close(pipes[j][0]);
                close(pipes[j][1]);
            }
            if (redirect_fd(&p->commands[i]) < 0) _exit(126);
            if (is_builtin(&p->commands[i])) _exit(execute_builtin(&p->commands[i]));
            execvp(p->commands[i].argv[0], p->commands[i].argv);
            perror(p->commands[i].argv[0]);
            _exit(127);
        }
        pids[started++] = pid;
    }

    for (size_t i = 0; i + 1 < n; ++i) {
        close(pipes[i][0]);
        close(pipes[i][1]);
    }
    free(pipes);

    if (p->background) {
        printf("[background pid %ld]\n", (long)pids[0]);
        free(pids);
        return 0;
    }
    return wait_for_all(pids, started);
}
