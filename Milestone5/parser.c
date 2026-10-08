#include "parser.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int add_arg(command_t *cmd, const char *s) {
    if (cmd->argc + 1 >= cmd->argv_capacity) {
        size_t nc = cmd->argv_capacity ? cmd->argv_capacity * 2 : 8;
        char **p = realloc(cmd->argv, nc * sizeof(*p));
        if (!p) return -1;
        cmd->argv = p; cmd->argv_capacity = nc;
    }
    cmd->argv[cmd->argc] = strdup(s);
    if (!cmd->argv[cmd->argc]) return -1;
    ++cmd->argc;
    cmd->argv[cmd->argc] = NULL;
    return 0;
}

static void command_init(command_t *c) { memset(c, 0, sizeof(*c)); }

void command_free(command_t *cmd) {
    for (size_t i = 0; i < cmd->argc; ++i) free(cmd->argv[i]);
    free(cmd->argv); free(cmd->input_file); free(cmd->output_file); free(cmd->error_file); command_init(cmd);
}

void pipeline_free(pipeline_t *p) {
    for (size_t i = 0; i < p->count; ++i) command_free(&p->commands[i]);
    free(p->command_line);
    free(p->commands); memset(p, 0, sizeof(*p));
}

static int add_command(pipeline_t *p) {
    if (p->count == p->capacity) {
        size_t nc = p->capacity ? p->capacity * 2 : 4;
        command_t *q = realloc(p->commands, nc * sizeof(*q));
        if (!q) return -1;
        p->commands = q; p->capacity = nc;
    }
    command_init(&p->commands[p->count++]);
    return 0;
}

int parse_tokens(const token_list_t *tokens, pipeline_t *out, char **error_message) {
    memset(out, 0, sizeof(*out)); *error_message = NULL;
    if (tokens->count == 0) return 0;
    if (add_command(out) < 0) goto oom;

    for (size_t i = 0; i < tokens->count; ++i) {
        const token_t *t = &tokens->tokens[i];
        command_t *cmd = &out->commands[out->count - 1];
        switch (t->type) {
            case TOKEN_WORD:
                if (add_arg(cmd, t->text) < 0) goto oom;
                break;
            case TOKEN_INPUT:
            case TOKEN_OUTPUT:
            case TOKEN_APPEND:
            case TOKEN_ERROR_OUTPUT:
                if (i + 1 >= tokens->count || tokens->tokens[i + 1].type != TOKEN_WORD) {
                    *error_message = strdup("redirection requires a filename"); goto fail;
                }
                if (t->type == TOKEN_INPUT) {
                    free(cmd->input_file); cmd->input_file = strdup(tokens->tokens[++i].text);
                    if (!cmd->input_file) goto oom;
                } else if (t->type == TOKEN_ERROR_OUTPUT) {
                    free(cmd->error_file); cmd->error_file = strdup(tokens->tokens[++i].text);
                    if (!cmd->error_file) goto oom;
                } else {
                    free(cmd->output_file); cmd->output_file = strdup(tokens->tokens[++i].text);
                    cmd->append_output = (t->type == TOKEN_APPEND);
                    if (!cmd->output_file) goto oom;
                }
                break;
            case TOKEN_DUP_STDERR:
                cmd->stderr_to_stdout = 1;
                break;
            case TOKEN_PIPE:
                if (cmd->argc == 0) { *error_message = strdup("empty command before pipe"); goto fail; }
                if (add_command(out) < 0) goto oom;
                break;
            case TOKEN_BACKGROUND:
                if (i != tokens->count - 1) { *error_message = strdup("& must be at the end"); goto fail; }
                out->background = 1;
                break;
        }
    }
    if (out->count == 0 || out->commands[out->count - 1].argc == 0) {
        *error_message = strdup("empty command"); goto fail;
    }
    return 0;

oom:
    *error_message = strdup("out of memory");
fail:
    pipeline_free(out); return -1;
}

void pipeline_print(const pipeline_t *p) {
    for (size_t i = 0; i < p->count; ++i) {
        printf("command %zu:", i + 1);
        for (size_t j = 0; j < p->commands[i].argc; ++j) printf(" [%s]", p->commands[i].argv[j]);
        if (p->commands[i].input_file) printf(" < %s", p->commands[i].input_file);
        if (p->commands[i].output_file) printf(" %s %s", p->commands[i].append_output ? ">>" : ">", p->commands[i].output_file);
        if (p->commands[i].error_file) printf(" 2> %s", p->commands[i].error_file);
        if (p->commands[i].stderr_to_stdout) printf(" 2>&1");
        putchar('\n');
    }
    if (p->background) puts("background: yes");
}
