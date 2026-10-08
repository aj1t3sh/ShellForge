#ifndef PARSER_H
#define PARSER_H

#include "token.h"
#include <stddef.h>

typedef struct {
    char **argv;
    size_t argc;
    size_t argv_capacity;
    char *input_file;
    char *output_file;
    char *error_file;
    int append_output;
    int stderr_to_stdout;
} command_t;

typedef struct {
    command_t *commands;
    size_t count;
    size_t capacity;
    int background;
} pipeline_t;

void command_free(command_t *cmd);
void pipeline_free(pipeline_t *p);
int parse_tokens(const token_list_t *tokens, pipeline_t *out, char **error_message);
void pipeline_print(const pipeline_t *p);

#endif
