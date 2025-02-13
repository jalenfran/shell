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

    char *input_ptr = input;
    char token_buffer[SHELL_MAX_INPUT];
    int token_pos = 0;
    int in_double_quotes = 0;
    int in_single_quotes = 0;
    command_t *current = cmd;
    int in_redir = 0, out_redir = 0;

    while (*input_ptr != '\0' && current->arg_count < MAX_ARGS - 1) {
        // Handle quotes
        if (*input_ptr == '"' && !in_single_quotes) {
            in_double_quotes = !in_double_quotes;
            input_ptr++;
            continue;
        }
        if (*input_ptr == '\'' && !in_double_quotes) {
            in_single_quotes = !in_single_quotes;
            input_ptr++;
            continue;
        }

        // If we're in quotes, copy character as-is
        if (in_double_quotes || in_single_quotes) {
            token_buffer[token_pos++] = *input_ptr++;
            continue;
        }

        // Check for special characters when not in quotes
        if (!in_double_quotes && !in_single_quotes) {
            if (*input_ptr == '|') {
                // Handle pipe
                if (token_pos > 0) {
                    token_buffer[token_pos] = '\0';
                    current->args[current->arg_count++] = strdup(token_buffer);
                    token_pos = 0;
                }
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
                input_ptr++;
                while (isspace(*input_ptr)) input_ptr++;
                continue;
            }
            else if (*input_ptr == '&') {
                // Handle background process
                if (token_pos > 0) {
                    token_buffer[token_pos] = '\0';
                    current->args[current->arg_count++] = strdup(token_buffer);
                    token_pos = 0;
                }
                current->background = 1;
                input_ptr++;
                continue;
            }
            else if (*input_ptr == '<') {
                // Handle input redirection
                if (token_pos > 0) {
                    token_buffer[token_pos] = '\0';
                    current->args[current->arg_count++] = strdup(token_buffer);
                    token_pos = 0;
                }
                in_redir = 1;
                input_ptr++;
                while (isspace(*input_ptr)) input_ptr++;
                continue;
            }
            else if (*input_ptr == '>') {
                // Handle output redirection
                if (token_pos > 0) {
                    token_buffer[token_pos] = '\0';
                    current->args[current->arg_count++] = strdup(token_buffer);
                    token_pos = 0;
                }
                input_ptr++;
                if (*input_ptr == '>') {
                    current->append_output = 1;
                    input_ptr++;
                }
                out_redir = 1;
                while (isspace(*input_ptr)) input_ptr++;
                continue;
            }
        }

        // Handle spaces and collect tokens
        if (isspace(*input_ptr)) {
            if (token_pos > 0) {
                token_buffer[token_pos] = '\0';
                char *expanded = expand_env_vars(token_buffer);  // Expand environment variables
                
                if (in_redir) {
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
                free(expanded);  // Free the expanded string
                token_pos = 0;
            }
            input_ptr++;
            continue;
        }

        // Add character to token
        token_buffer[token_pos++] = *input_ptr++;
    }

    // Handle the last token
    if (token_pos > 0) {
        token_buffer[token_pos] = '\0';
        char *expanded = expand_env_vars(token_buffer);  // Expand environment variables
        
        if (in_redir) {
            current->input_file = strdup(expanded);
        } else if (out_redir) {
            current->output_file = strdup(expanded);
        } else {
            current->args[current->arg_count] = strdup(expanded);
            if (!current->command) {
                current->command = strdup(expanded);
            }
            current->arg_count++;
        }
        free(expanded);  // Free the expanded string
    }

    // Null terminate argument list
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
