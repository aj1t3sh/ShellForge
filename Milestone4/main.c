#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <sys/wait.h>
#include <readline/readline.h>
#include <readline/history.h>
#include "token.h"
#include "parser.h"
#include "expand.h"
#include "executor.h"

static void reap_background(void) {
    int status;
    while (waitpid(-1, &status, WNOHANG) > 0) { }
}

int main(void) {
    using_history();
    signal(SIGINT, SIG_IGN);
    signal(SIGQUIT, SIG_IGN);
    puts("ShellForge - Milestone 4");
    puts("Supports: built-ins, external commands, pipelines, <, >, >>, and &");

    for (;;) {
        reap_background();
        char *line = readline("shellforge$ ");
        if (!line) { putchar('\n'); break; }
        if (!*line) { free(line); continue; }
        add_history(line);

        token_list_t tokens;
        pipeline_t pipeline;
        char *error = NULL;
        if (tokenize(line, &tokens, &error) < 0) {
            fprintf(stderr, "lex error: %s\n", error ? error : "unknown error");
            free(error); free(line); continue;
        }
        if (parse_tokens(&tokens, &pipeline, &error) < 0) {
            fprintf(stderr, "parse error: %s\n", error ? error : "unknown error");
            free(error); token_list_free(&tokens); free(line); continue;
        }
        if (expand_pipeline(&pipeline) < 0) {
            fprintf(stderr, "expansion error\n");
        } else {
            int rc = execute_pipeline(&pipeline);
            if (rc == 99 && pipeline.count == 1 && pipeline.commands[0].argc > 0 && strcmp(pipeline.commands[0].argv[0], "exit") == 0) {
                pipeline_free(&pipeline); token_list_free(&tokens); free(error); free(line); break;
            }
        }
        pipeline_free(&pipeline);
        token_list_free(&tokens);
        free(error);
        free(line);
    }
    return 0;
}
