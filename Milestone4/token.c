#include "token.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void token_list_init(token_list_t *list) {
    list->tokens = NULL;
    list->count = 0;
    list->capacity = 0;
}

void token_list_free(token_list_t *list) {
    for (size_t i = 0; i < list->count; ++i) free(list->tokens[i].text);
    free(list->tokens);
    token_list_init(list);
}

static int push_token(token_list_t *list, token_type_t type, const char *text) {
    if (list->count == list->capacity) {
        size_t new_cap = list->capacity ? list->capacity * 2 : 16;
        token_t *p = realloc(list->tokens, new_cap * sizeof(*p));
        if (!p) return -1;
        list->tokens = p;
        list->capacity = new_cap;
    }
    list->tokens[list->count].type = type;
    list->tokens[list->count].text = strdup(text ? text : "");
    if (!list->tokens[list->count].text) return -1;
    ++list->count;
    return 0;
}

static int push_char(char **buf, size_t *len, size_t *cap, char c) {
    if (*len + 1 >= *cap) {
        size_t new_cap = *cap ? *cap * 2 : 32;
        char *p = realloc(*buf, new_cap);
        if (!p) return -1;
        *buf = p;
        *cap = new_cap;
    }
    (*buf)[(*len)++] = c;
    (*buf)[*len] = '\0';
    return 0;
}

static int finish_word(token_list_t *out, char *buf, size_t len) {
    return len == 0 ? 0 : push_token(out, TOKEN_WORD, buf);
}

int tokenize(const char *line, token_list_t *out, char **error_message) {
    token_list_init(out);
    *error_message = NULL;
    size_t i = 0, len = 0, cap = 0;
    char *buf = NULL;

    while (line[i]) {
        unsigned char c = (unsigned char)line[i];

        if (c == ' ' || c == '\t' || c == '\n') {
            if (finish_word(out, buf, len) < 0) goto oom;
            len = 0; if (buf) buf[0] = '\0';
            ++i;
            continue;
        }

        if (c == '\'' || c == '"') {
            char quote = (char)c;
            int closed = 0;
            ++i;
            while (line[i]) {
                if (line[i] == quote) { closed = 1; ++i; break; }
                if (quote == '"' && line[i] == '\\' && line[i + 1]) ++i;
                if (push_char(&buf, &len, &cap, line[i]) < 0) goto oom;
                ++i;
            }
            if (!closed) {
                *error_message = strdup("unterminated quote");
                free(buf); token_list_free(out); return -1;
            }
            continue;
        }

        if (c == '\\') {
            if (!line[i + 1]) {
                *error_message = strdup("trailing backslash");
                free(buf); token_list_free(out); return -1;
            }
            ++i;
            if (push_char(&buf, &len, &cap, line[i]) < 0) goto oom;
            ++i;
            continue;
        }

        if (c == '|') {
            if (finish_word(out, buf, len) < 0 || push_token(out, TOKEN_PIPE, "|") < 0) goto oom;
            len = 0; if (buf) buf[0] = '\0'; ++i;
            continue;
        }
        if (c == '<') {
            if (finish_word(out, buf, len) < 0 || push_token(out, TOKEN_INPUT, "<") < 0) goto oom;
            len = 0; if (buf) buf[0] = '\0'; ++i;
            continue;
        }
        if (c == '2' && line[i + 1] == '>') {
            if (finish_word(out, buf, len) < 0 || push_token(out, TOKEN_ERROR_OUTPUT, "2>") < 0) goto oom;
            len = 0; if (buf) buf[0] = '\0';
            i += 2;
            if (line[i] == '&' && line[i + 1] == '1') {
                if (push_token(out, TOKEN_DUP_STDERR, "2>&1") < 0) goto oom;
                i += 2;
            }
            continue;
        }
        if (c == '>') {
            if (finish_word(out, buf, len) < 0) goto oom;
            if (line[i + 1] == '>') {
                if (push_token(out, TOKEN_APPEND, ">>") < 0) goto oom;
                i += 2;
            } else {
                if (push_token(out, TOKEN_OUTPUT, ">") < 0) goto oom;
                ++i;
            }
            len = 0; if (buf) buf[0] = '\0';
            continue;
        }
        if (c == '&') {
            if (finish_word(out, buf, len) < 0 || push_token(out, TOKEN_BACKGROUND, "&") < 0) goto oom;
            len = 0; if (buf) buf[0] = '\0'; ++i;
            continue;
        }

        if (push_char(&buf, &len, &cap, (char)c) < 0) goto oom;
        ++i;
    }

    if (finish_word(out, buf, len) < 0) goto oom;
    free(buf);
    return 0;

oom:
    free(buf);
    token_list_free(out);
    *error_message = strdup("out of memory");
    return -1;
}

const char *token_type_name(token_type_t type) {
    switch (type) {
        case TOKEN_WORD: return "WORD";
        case TOKEN_PIPE: return "PIPE";
        case TOKEN_INPUT: return "INPUT";
        case TOKEN_OUTPUT: return "OUTPUT";
        case TOKEN_APPEND: return "APPEND";
        case TOKEN_ERROR_OUTPUT: return "ERROR_OUTPUT";
        case TOKEN_DUP_STDERR: return "DUP_STDERR";
        case TOKEN_BACKGROUND: return "BACKGROUND";
    }
    return "UNKNOWN";
}

void token_print(const token_list_t *list) {
    for (size_t i = 0; i < list->count; ++i)
        printf("%-12s %s\n", token_type_name(list->tokens[i].type), list->tokens[i].text);
}
