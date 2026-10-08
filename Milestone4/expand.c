#include "expand.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

static char *expand_text(const char *s) {
    size_t cap = strlen(s) + 1, len = 0;
    char *out = malloc(cap); if (!out) return NULL;
    for (size_t i = 0; s[i]; ) {
        if (s[i] != '$') {
            if (len + 2 > cap) { cap *= 2; out = realloc(out, cap); if (!out) return NULL; }
            out[len++] = s[i++]; continue;
        }
        ++i;
        if (s[i] == '{') {
            ++i; size_t start = i; while (s[i] && s[i] != '}') ++i;
            if (s[i] != '}') { free(out); return NULL; }
            char *name = strndup(s + start, i - start); if (!name) { free(out); return NULL; }
            const char *value = getenv(name); free(name); ++i;
            if (!value) value = "";
            size_t n = strlen(value); while (len + n + 1 > cap) cap *= 2;
            char *tmp = realloc(out, cap); if (!tmp) { free(out); return NULL; } out = tmp;
            memcpy(out + len, value, n); len += n; continue;
        }
        size_t start = i;
        while (isalnum((unsigned char)s[i]) || s[i] == '_') ++i;
        if (start == i) { if (len + 2 > cap) { cap *= 2; out = realloc(out, cap); if (!out) return NULL; } out[len++] = '$'; continue; }
        char *name = strndup(s + start, i - start); if (!name) { free(out); return NULL; }
        const char *value = getenv(name); free(name); if (!value) value = "";
        size_t n = strlen(value); while (len + n + 1 > cap) cap *= 2;
        char *tmp = realloc(out, cap); if (!tmp) { free(out); return NULL; } out = tmp;
        memcpy(out + len, value, n); len += n;
    }
    out[len] = '\0'; return out;
}

int expand_pipeline(pipeline_t *p) {
    for (size_t i = 0; i < p->count; ++i) {
        command_t *c = &p->commands[i];
        for (size_t j = 0; j < c->argc; ++j) {
            char *x = expand_text(c->argv[j]); if (!x) return -1;
            free(c->argv[j]); c->argv[j] = x;
        }
        if (c->input_file) { char *x = expand_text(c->input_file); if (!x) return -1; free(c->input_file); c->input_file = x; }
        if (c->output_file) { char *x = expand_text(c->output_file); if (!x) return -1; free(c->output_file); c->output_file = x; }
        if (c->error_file) { char *x = expand_text(c->error_file); if (!x) return -1; free(c->error_file); c->error_file = x; }
    }
    return 0;
}
