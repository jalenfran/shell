#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "shell.h"
#include "command.h"
#include <stdlib.h> // for getenv

// Updated tokenize: also split on '|', '<', '>' (handle ">>" as one token)
static char **tokenize(const char *input, int *count) {
    char **tokens = malloc(sizeof(char*) * 256);
    *count = 0;
    const char *p = input;
    while (*p) {
        while (isspace(*p)) p++;
        if (!*p) break;
        // Check for control characters outside quotes
        if (*p == ';' || *p == '&' || *p == '|' || *p == '<' || *p == '>') {
            // For ">>", check next character
            if (*p == '>' && *(p+1) == '>') {
                tokens[(*count)++] = strdup(">>");
                p += 2;
            } else {
                char token[2] = {*p, '\0'};
                tokens[(*count)++] = strdup(token);
                p++;
            }
            continue;
        }
        char token[1024] = {0};
        int t = 0;
        if (*p == '"' || *p == '\'') {
            char quote = *p++;
            while (*p && *p != quote) {
                token[t++] = *p++;
            }
            if (*p == quote) p++;
        } else {
            while (*p && !isspace(*p) && *p != ';' && *p != '&' && *p != '|' && *p != '<' && *p != '>') {
                token[t++] = *p++;
            }
        }
        token[t] = '\0';
        tokens[(*count)++] = strdup(token);
    }
    tokens[*count] = NULL;
    return tokens;
}

// New helper function to expand environment variables in a token.
static char *expand_variables_in_token(const char *token) {
    if (token && token[0] == '$') {
        char *env_val = getenv(token + 1);
        return env_val ? strdup(env_val) : strdup("");
    }
    return strdup(token);
}

// Forward declarations for recursive descent
static command_t *parse_command(char **tokens, int *pos, int count);
static command_t *parse_if(char **tokens, int *pos, int count);
static command_t *parse_simple(char **tokens, int *pos, int count);

// parse_input: tokenizes then parses recursively.
command_t *parse_input(char *input) {
    int count = 0;
    char **tokens = tokenize(input, &count);
    int pos = 0;
    command_t *cmd = parse_command(tokens, &pos, count);
    // Free tokens array (each token was strdup’ed)
    for (int i = 0; i < count; i++) free(tokens[i]);
    free(tokens);
    return cmd;
}

// Updated parse_command: dispatches based on tokens and chains pipelines.
static command_t *parse_command(char **tokens, int *pos, int count) {
    command_t *cmd;
    if (*pos < count && strcmp(tokens[*pos], "if") == 0) {
        cmd = parse_if(tokens, pos, count);
    } else {
        cmd = parse_simple(tokens, pos, count);
    }
    // Check if next token is a pipe, then build pipeline chain.
    if (*pos < count && strcmp(tokens[*pos], "|") == 0) {
        (*pos)++; // skip "|"
        cmd->next = parse_command(tokens, pos, count);
    }
    return cmd;
}

// Modified parse_if: collects condition tokens until "then" (ignoring a semicolon if it immediately precedes "then")
static command_t *parse_if(char **tokens, int *pos, int count) {
    command_t *cmd = malloc(sizeof(command_t));
    memset(cmd, 0, sizeof(command_t));
    cmd->type = CMD_IF;
    
    // Skip "if"
    (*pos)++;
    
    // Collect condition tokens until an un-nested "then" is encountered.
    int nested = 0;
    char cond[1024] = {0};
    while (*pos < count) {
        if (strcmp(tokens[*pos], "if") == 0) {
            nested++;
        } else if (strcmp(tokens[*pos], "fi") == 0) {
            if (nested > 0)
                nested--;
        }
        // Only break on "then" if no nested if is active.
        if (nested == 0 && strcmp(tokens[*pos], "then") == 0) {
            break;
        }
        strcat(cond, tokens[*pos]);
        strcat(cond, " ");
        (*pos)++;
    }
    cmd->if_condition = strdup(cond);
    
    // Expect "then"
    if (*pos < count && strcmp(tokens[*pos], "then") == 0) {
        (*pos)++;
    }
    // Parse then branch
    cmd->then_branch = parse_command(tokens, pos, count);
    
    // Skip extra semicolon delimiter after then branch if present.
    if (*pos < count && strcmp(tokens[*pos], ";") == 0) {
        (*pos)++;
    }
    
    // Check for optional "else"
    if (*pos < count && strcmp(tokens[*pos], "else") == 0) {
        (*pos)++;
        cmd->else_branch = parse_command(tokens, pos, count);
    }
    // Expect "fi"
    if (*pos < count && strcmp(tokens[*pos], "fi") == 0) {
        (*pos)++;
    }
    return cmd;
}

// Updated parse_simple: detect redirection tokens, background marker, and expand variables.
static command_t *parse_simple(char **tokens, int *pos, int count) {
    command_t *cmd = malloc(sizeof(command_t));
    memset(cmd, 0, sizeof(command_t));
    cmd->type = CMD_SIMPLE;
    
    // Allocate args array
    cmd->args = malloc(sizeof(char*) * 64);
    cmd->arg_count = 0;
    
    while (*pos < count) {
        // Stop if a control token is encountered (if, then, else, fi, ;, |) 
        if (strcmp(tokens[*pos], "if") == 0 ||
            strcmp(tokens[*pos], "then") == 0 ||
            strcmp(tokens[*pos], "else") == 0 ||
            strcmp(tokens[*pos], "fi") == 0 ||
            strcmp(tokens[*pos], ";") == 0 ||
            strcmp(tokens[*pos], "|") == 0) {
            break;
        }
        // Check for background token '&'
        if (strcmp(tokens[*pos], "&") == 0) {
            cmd->background = 1;
            (*pos)++;
            continue;
        }
        // Handle input redirection.
        if (strcmp(tokens[*pos], "<") == 0) {
            (*pos)++;
            if (*pos < count) {
                cmd->input_file = strdup(tokens[*pos]);
                (*pos)++;
                continue;
            }
        }
        // Handle output redirection.
        if (strcmp(tokens[*pos], ">") == 0 || strcmp(tokens[*pos], ">>") == 0) {
            int is_append = (strcmp(tokens[*pos], ">>") == 0);
            (*pos)++;
            if (*pos < count) {
                cmd->output_file = strdup(tokens[*pos]);
                cmd->append_output = is_append;
                (*pos)++;
                continue;
            }
        }
        // For simplicity, treat each token as an argument with variable expansion.
        char *expanded = expand_variables_in_token(tokens[*pos]);
        cmd->args[cmd->arg_count++] = expanded;
        (*pos)++;
    }
    cmd->args[cmd->arg_count] = NULL;
    if (cmd->arg_count > 0)
        cmd->command = strdup(cmd->args[0]);
    return cmd;
}

void command_free(command_t *cmd) {
    if (!cmd) return;
    if (cmd->type == CMD_SIMPLE) {
        for (int i = 0; i < cmd->arg_count; i++)
            free(cmd->args[i]);
        free(cmd->args);
        free(cmd->command);
        free(cmd->input_file);
        free(cmd->output_file);
    } else if (cmd->type == CMD_IF) {
        free(cmd->if_condition);
        command_free(cmd->then_branch);
        if (cmd->else_branch) command_free(cmd->else_branch);
    }
    if (cmd->next) command_free(cmd->next);
    free(cmd);
}
