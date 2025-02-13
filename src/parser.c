#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "shell.h"

static char *expand_env_vars(const char *token) {
    if (!token || !strchr(token, '$')) {
        return strdup(token);
    }

    char *result = malloc(SHELL_MAX_INPUT);
    int j = 0;

    for (int i = 0; token[i]; i++) {
        if (token[i] == '$' && token[i + 1]) {
            i++;
            char varname[256] = {0};
            int k = 0;
            while (token[i] && (isalnum(token[i]) || token[i] == '_')) {
                varname[k++] = token[i++];
            }
            i--;
            char *value = getenv(varname);
            if (value) {
                strcpy(result + j, value);
                j += strlen(value);
            }
        } else {
            result[j++] = token[i];
        }
    }
    result[j] = '\0';
    return result;
}

command_t *parse_input(char *input) {
    command_t *cmd = malloc(sizeof(command_t));
    if (!cmd) {
        perror("malloc failed");
        return NULL;
    }

    cmd->args = malloc(MAX_ARGS * sizeof(char *));
    if (!cmd->args) {
        free(cmd);
        perror("malloc failed");
        return NULL;
    }

    // Initialize command structure
    cmd->arg_count = 0;
    cmd->command = NULL;
    cmd->input_file = NULL;
    cmd->output_file = NULL;
    cmd->append_output = 0;
    cmd->background = 0;
    cmd->next = NULL;

    char *token;
    int in_redir = 0, out_redir = 0;
    command_t *current = cmd;
    char *input_ptr = input;
    char token_buffer[SHELL_MAX_INPUT];
    int token_pos = 0;
    int in_quotes = 0;

    while (*input_ptr && current->arg_count < MAX_ARGS - 1) {
        if (*input_ptr == '"') {
            in_quotes = !in_quotes;
            input_ptr++;
            continue;
        }

        if (in_quotes) {
            token_buffer[token_pos++] = *input_ptr++;
            continue;
        }

        if (isspace(*input_ptr) && token_pos > 0) {
            // End of token
            token_buffer[token_pos] = '\0';
            token_pos = 0;
            
            // Process the token
            char *expanded = expand_env_vars(token_buffer);
            
            if (strcmp(expanded, "|") == 0) {
                // Handle pipe
                current->args[current->arg_count] = NULL;
                current->next = malloc(sizeof(command_t));
                current = current->next;
                current->args = malloc(MAX_ARGS * sizeof(char *));
                current->arg_count = 0;
                current->command = NULL;
                current->input_file = NULL;
                current->output_file = NULL;
                current->append_output = 0;
                current->background = 0;
                current->next = NULL;
            } else if (strcmp(expanded, "<") == 0) {
                in_redir = 1;
            } else if (strcmp(expanded, ">") == 0) {
                out_redir = 1;
            } else if (strcmp(expanded, ">>") == 0) {
                out_redir = 1;
                current->append_output = 1;
            } else if (strcmp(expanded, "&") == 0) {
                current->background = 1;
            } else if (in_redir) {
                current->input_file = strdup(expanded);
                in_redir = 0;
            } else if (out_redir) {
                current->output_file = strdup(expanded);
                out_redir = 0;
            } else {
                current->args[current->arg_count] = strdup(expanded);
                if (!current->command) {
                    current->command = strdup(expanded);
                }
                current->arg_count++;
            }
            free(expanded);

            while (isspace(*input_ptr)) input_ptr++;
            continue;
        }

        if (!isspace(*input_ptr)) {
            token_buffer[token_pos++] = *input_ptr;
        }
        input_ptr++;
    }

    // Handle last token if exists
    if (token_pos > 0) {
        token_buffer[token_pos] = '\0';
        char *expanded = expand_env_vars(token_buffer);
        current->args[current->arg_count] = strdup(expanded);
        if (!current->command) {
            current->command = strdup(expanded);
        }
        current->arg_count++;
        free(expanded);
    }

    current->args[current->arg_count] = NULL;
    return cmd;
}

void command_free(command_t *cmd) {
    if (cmd) {
        free(cmd->args);
        free(cmd->input_file);
        free(cmd->output_file);
        if (cmd->next) {
            command_free(cmd->next);
        }
        free(cmd);
    }
}
