#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <readline/readline.h>
#include <readline/history.h>
#include "token.h"
#include "parser.h"
#include "expand.h"
#include "executor.h"
#include "jobs.h"

int main(void) {
    if (jobs_init() < 0) return 1;
    using_history();
    puts("ShellForge - Milestone 5");
    puts("Job control: jobs, fg, bg, SIGCHLD, SIGCONT, Ctrl-Z");

    for (;;) {
        jobs_reap();
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
        pipeline.command_line = strdup(line);
        if (!pipeline.command_line) {
            fprintf(stderr, "out of memory\n");
            pipeline_free(&pipeline); token_list_free(&tokens); free(error); free(line); continue;
        }

        if (expand_pipeline(&pipeline) < 0) {
            fprintf(stderr, "expansion error\n");
        } else {
            int rc = execute_pipeline(&pipeline);
            if (rc == 99 && pipeline.count == 1 && pipeline.commands[0].argc > 0 &&
                strcmp(pipeline.commands[0].argv[0], "exit") == 0) {
                pipeline_free(&pipeline); token_list_free(&tokens); free(error); free(line); break;
            }
        }
        pipeline_free(&pipeline);
        token_list_free(&tokens);
        free(error);
        free(line);
    }

    jobs_reap();
    jobs_shutdown();
    return 0;
}
