#include <stdio.h>
#include <stdlib.h>
#include <readline/readline.h>
#include "token.h"
#include "parser.h"
#include "expand.h"

int main(void) {
    puts("ShellForge - Milestone 2 lexer/parser/expansion test");
    char *line;
    while ((line = readline("m2$ ")) != NULL) {
        if (*line) {
            token_list_t tokens; char *err = NULL;
            if (tokenize(line, &tokens, &err) == 0) {
                pipeline_t p;
                if (parse_tokens(&tokens, &p, &err) == 0) {
                    if (expand_pipeline(&p) == 0) pipeline_print(&p);
                    else puts("expansion error");
                    pipeline_free(&p);
                } else printf("parse error: %s\n", err);
                token_list_free(&tokens);
            } else printf("lex error: %s\n", err);
            free(err);
        }
        free(line);
    }
    return 0;
}
