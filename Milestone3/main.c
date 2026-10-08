#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <readline/readline.h>
#include <readline/history.h>
#include "token.h"
#include "parser.h"
#include "expand.h"
#include "builtin.h"
#include "executor.h"

int main(void) {
    using_history();
    puts("ShellForge - Milestone 3");
    for (;;) {
        char *line = readline("shellforge$ ");
        if (!line) { putchar('\n'); break; }
        if (!*line) { free(line); continue; }
        add_history(line);
        token_list_t tokens; pipeline_t p; char *err = NULL;
        if (tokenize(line, &tokens, &err) < 0) { fprintf(stderr, "lex error: %s\n", err); free(err); free(line); continue; }
        if (parse_tokens(&tokens, &p, &err) < 0) { fprintf(stderr, "parse error: %s\n", err); free(err); token_list_free(&tokens); free(line); continue; }
        if (expand_pipeline(&p) < 0) { fprintf(stderr, "expansion error\n"); pipeline_free(&p); token_list_free(&tokens); free(line); continue; }
        if (p.count != 1) fprintf(stderr, "Milestone 3 supports one command at a time\n");
        else if (is_builtin(&p.commands[0])) { int rc = execute_builtin(&p.commands[0]); if (rc == 99) { pipeline_free(&p); token_list_free(&tokens); free(line); break; } }
        else execute_command(&p.commands[0]);
        pipeline_free(&p); token_list_free(&tokens); free(line);
    }
    return 0;
}
