#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <readline/readline.h>
#include <readline/history.h>
#include "history.h"

int main(void) {
    using_history();
    printf("ShellForge - Milestone 1\n");

    while (1) {
        char *line = readline("shellforge$ ");
        if (line == NULL) {
            putchar('\n');
            break;
        }
        if (*line == '\0') {
            free(line);
            continue;
        }
        if (strcmp(line, "history") == 0) {
            print_history();
            free(line);
            continue;
        }
        add_history(line);
        if (strcmp(line, "exit") == 0) {
            free(line);
            break;
        }
        printf("YOU ENTERED: %s\n", line);
        free(line);
    }
    return 0;
}
