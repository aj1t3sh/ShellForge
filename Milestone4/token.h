#ifndef TOKEN_H
#define TOKEN_H

#include <stddef.h>

typedef enum {
    TOKEN_WORD,
    TOKEN_PIPE,
    TOKEN_INPUT,
    TOKEN_OUTPUT,
    TOKEN_APPEND,
    TOKEN_ERROR_OUTPUT,
    TOKEN_DUP_STDERR,
    TOKEN_BACKGROUND
} token_type_t;

typedef struct {
    token_type_t type;
    char *text;
} token_t;

typedef struct {
    token_t *tokens;
    size_t count;
    size_t capacity;
} token_list_t;

void token_list_init(token_list_t *list);
void token_list_free(token_list_t *list);
int tokenize(const char *line, token_list_t *out, char **error_message);
const char *token_type_name(token_type_t type);
void token_print(const token_list_t *list);

#endif
