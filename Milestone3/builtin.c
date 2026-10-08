#include "builtin.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>

int is_builtin(const command_t *c) {
    if (!c || c->argc == 0) return 0;
    return !strcmp(c->argv[0], "cd") || !strcmp(c->argv[0], "pwd") || !strcmp(c->argv[0], "echo") || !strcmp(c->argv[0], "exit") || !strcmp(c->argv[0], "history");
}

int execute_builtin(const command_t *c) {
    if (!strcmp(c->argv[0], "cd")) {
        if (c->argc > 2) { fprintf(stderr, "cd: too many arguments\n"); return 1; }
        const char *path = c->argc == 1 ? getenv("HOME") : c->argv[1];
        if (!path) { fprintf(stderr, "cd: HOME is not set\n"); return 1; }
        if (chdir(path) != 0) { perror("cd"); return 1; }
        return 0;
    }
    if (!strcmp(c->argv[0], "pwd")) {
        if (c->argc != 1) { fprintf(stderr, "pwd: no arguments expected\n"); return 1; }
        char cwd[PATH_MAX]; if (!getcwd(cwd, sizeof(cwd))) { perror("pwd"); return 1; }
        puts(cwd); return 0;
    }
    if (!strcmp(c->argv[0], "echo")) {
        for (size_t i = 1; i < c->argc; ++i) printf("%s%s", i == 1 ? "" : " ", c->argv[i]);
        putchar('\n'); return 0;
    }
    if (!strcmp(c->argv[0], "history")) { puts("history is handled by the shell main loop"); return 0; }
    if (!strcmp(c->argv[0], "exit")) return 99;
    return 1;
}
