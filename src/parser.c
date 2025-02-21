#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "shell.h"
#include "command.h"

// Tokenizes the input into an array of tokens.
static char **tokenize(const char *input, int *count) {
    char **tokens = malloc(sizeof(char*) * 256);
    *count = 0;
    const char *p = input;
    while (*p) {
        while (isspace(*p)) p++;
        if (!*p) break;
        if (*p == ';' || *p == '&' || *p == '|' || *p == '<' || *p == '>' || *p == '(' || *p == ')') {
            if (*p == ';' && *(p+1) == ';') {
                tokens[(*count)++] = strdup(";;");
                p += 2;
            }
            else if ((*p == '&' && *(p+1) == '&') || (*p == '|' && *(p+1) == '|')) {
                char token[3] = {*p, *p, '\0'};
                tokens[(*count)++] = strdup(token);
                p += 2;
            }
            else if (*p == '>' && *(p+1) == '>') {
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
            while (*p && *p != quote)
                token[t++] = *p++;
            if (*p == quote) p++;
        } else {
            while (*p && !isspace(*p) &&
                   *p != ';' && *p != '&' && *p != '|' &&
                   *p != '<' && *p != '>' && *p != '(' && *p != ')') {
                if (*p == '*' && *(p+1) == ')') {
                    token[t++] = *p++;
                    token[t++] = *p++;
                    break;
                }
                token[t++] = *p++;
            }
        }
        token[t] = '\0';
        tokens[(*count)++] = strdup(token);
    }
    tokens[*count] = NULL;
    return tokens;
}

// Forward declarations
static command_t *parse_command(char **tokens, int *pos, int count);
static command_t *parse_if(char **tokens, int *pos, int count);
static command_t *parse_while(char **tokens, int *pos, int count);
static command_t *parse_for(char **tokens, int *pos, int count);
static command_t *parse_case(char **tokens, int *pos, int count);
static command_t *parse_simple(char **tokens, int *pos, int count);
static command_t *parse_subshell(char **tokens, int *pos, int count);
static command_t *parse_pipeline(char **tokens, int *pos, int count);

command_t *parse_input(char *input) {
    int count = 0;
    char **tokens = tokenize(input, &count);
    int pos = 0;
    command_t *cmd = parse_command(tokens, &pos, count);
    for (int i = 0; i < count; i++) free(tokens[i]);
    free(tokens);
    return cmd;
}

static command_t *parse_command(char **tokens, int *pos, int count) {
    if (*pos >= count) return NULL;
    if (strcmp(tokens[*pos], "if") == 0) return parse_if(tokens, pos, count);
    if (strcmp(tokens[*pos], "while") == 0) return parse_while(tokens, pos, count);
    if (strcmp(tokens[*pos], "for") == 0) return parse_for(tokens, pos, count);
    if (strcmp(tokens[*pos], "case") == 0) return parse_case(tokens, pos, count);
    command_t *cmd = parse_pipeline(tokens, pos, count);
    if (*pos < count && strcmp(tokens[*pos], ";") == 0) {
        (*pos)++;
        command_t *seq = malloc(sizeof(command_t));
        memset(seq, 0, sizeof(command_t));
        seq->type = CMD_SEQUENCE;
        seq->then_branch = cmd;
        seq->else_branch = parse_command(tokens, pos, count);
        return seq;
    }
    else if (*pos < count && (strcmp(tokens[*pos], "&&") == 0 || strcmp(tokens[*pos], "||") == 0)) {
        command_t *logical_cmd = malloc(sizeof(command_t));
        memset(logical_cmd, 0, sizeof(command_t));
        logical_cmd->type = (strcmp(tokens[*pos], "&&") == 0) ? CMD_AND : CMD_OR;
        (*pos)++;
        logical_cmd->then_branch = cmd;
        logical_cmd->else_branch = parse_command(tokens, pos, count);
        return logical_cmd;
    }
    return cmd;
}

static command_t *parse_if(char **tokens, int *pos, int count) {
    command_t *cmd = malloc(sizeof(command_t));
    memset(cmd, 0, sizeof(command_t));
    cmd->type = CMD_IF;
    (*pos)++; // Skip "if"
    int nested = 0;
    char cond[1024] = {0};
    while (*pos < count) {
        if (strcmp(tokens[*pos], "if") == 0) nested++;
        else if (strcmp(tokens[*pos], "fi") == 0 && nested > 0) nested--;
        if (nested == 0 && strcmp(tokens[*pos], "then") == 0) break;
        strcat(cond, tokens[*pos]);
        strcat(cond, " ");
        (*pos)++;
    }
    cmd->if_condition = strdup(cond);
    if (*pos < count && strcmp(tokens[*pos], "then") == 0) (*pos)++;
    cmd->then_branch = parse_command(tokens, pos, count);
    if (*pos < count && strcmp(tokens[*pos], ";") == 0) (*pos)++;
    if (*pos < count && strcmp(tokens[*pos], "else") == 0) {
        (*pos)++;
        if (*pos < count && strcmp(tokens[*pos], "if") == 0) cmd->else_branch = parse_if(tokens, pos, count);
        else cmd->else_branch = parse_command(tokens, pos, count);
    }
    if (*pos < count && strcmp(tokens[*pos], "fi") == 0) (*pos)++;
    return cmd;
}

static command_t *parse_while(char **tokens, int *pos, int count) {
    command_t *cmd = malloc(sizeof(command_t));
    memset(cmd, 0, sizeof(command_t));
    cmd->type = CMD_WHILE;
    (*pos)++;
    char cond[1024] = {0};
    while (*pos < count && strcmp(tokens[*pos], "do") != 0) {
        strcat(cond, tokens[*pos]);
        strcat(cond, " ");
        (*pos)++;
    }
    cmd->while_condition = strdup(cond);
    if (*pos < count && strcmp(tokens[*pos], "do") == 0) (*pos)++;
    cmd->while_body = parse_command(tokens, pos, count);
    if (*pos < count && strcmp(tokens[*pos], "done") == 0) (*pos)++;
    return cmd;
}

static command_t *parse_for(char **tokens, int *pos, int count) {
    command_t *cmd = malloc(sizeof(command_t));
    memset(cmd, 0, sizeof(command_t));
    cmd->type = CMD_FOR;
    (*pos)++;
    if (*pos < count) {
        cmd->for_variable = strdup(tokens[*pos]);
        (*pos)++;
    }
    if (*pos < count && strcmp(tokens[*pos], "in") == 0) (*pos)++;
    char **list = malloc(sizeof(char*) * 64);
    int list_count = 0;
    while (*pos < count && strcmp(tokens[*pos], "do") != 0) {
        list[list_count++] = strdup(tokens[*pos]);
        (*pos)++;
    }
    list[list_count] = NULL;
    cmd->for_list = list;
    if (*pos < count && strcmp(tokens[*pos], "do") == 0) (*pos)++;
    cmd->for_body = parse_command(tokens, pos, count);
    if (*pos < count && strcmp(tokens[*pos], "done") == 0) (*pos)++;
    return cmd;
}

static command_t *parse_case_body_simple(char **tokens, int *pos, int count) {
    int start = *pos;
    while (*pos < count) {
        if (strcmp(tokens[*pos], "esac") == 0)
            break;
        if (strcmp(tokens[*pos], ";;") == 0)
            break;
        if (strcmp(tokens[*pos], ";") == 0) {
            if (*pos + 1 < count && strcmp(tokens[*pos + 1], ";;") == 0)
                break;
        }
        (*pos)++;
    }
    char body_str[4096] = "";
    for (int i = start; i < *pos; i++) {
        strcat(body_str, tokens[i]);
        if (i < *pos - 1) {
            strcat(body_str, " ");
        }
    }
    command_t *cmd = malloc(sizeof(command_t));
    memset(cmd, 0, sizeof(command_t));
    cmd->type = CMD_SIMPLE;
    cmd->command = strdup(body_str);
    cmd->args = malloc(sizeof(char*) * 64);
    cmd->arg_count = 0;
    char *temp = strdup(body_str);
    char *tok = strtok(temp, " ");
    while (tok) {
        cmd->args[cmd->arg_count++] = strdup(tok);
        tok = strtok(NULL, " ");
    }
    cmd->args[cmd->arg_count] = NULL;
    free(temp);
    return cmd;
}

static command_t *parse_case(char **tokens, int *pos, int count) {
    command_t *cmd = malloc(sizeof(command_t));
    memset(cmd, 0, sizeof(command_t));
    cmd->type = CMD_CASE;
    (*pos)++;
    if (*pos < count) {
        cmd->case_expression = strdup(tokens[*pos]);
        (*pos)++;
    }
    if (*pos < count && strcmp(tokens[*pos], "in") == 0) (*pos)++;
    cmd->case_entries = NULL;
    cmd->case_entry_count = 0;
    while (*pos < count && strcmp(tokens[*pos], "esac") != 0) {
        while (*pos < count && strcmp(tokens[*pos], ";;") == 0) (*pos)++;
        if (*pos >= count || strcmp(tokens[*pos], "esac") == 0) break;
        char *pattern = strdup(tokens[*pos]);
        size_t len = strlen(pattern);
        if (len > 0 && pattern[len - 1] == ')')
            pattern[len - 1] = '\0';
        (*pos)++;
        if (*pos < count && strcmp(tokens[*pos], ")") == 0) (*pos)++;
        command_t *body = parse_case_body_simple(tokens, pos, count);
        cmd->case_entries = realloc(cmd->case_entries, sizeof(case_entry_t*) * (cmd->case_entry_count + 1));
        case_entry_t *entry = malloc(sizeof(case_entry_t));
        entry->pattern = pattern;
        entry->body = body;
        cmd->case_entries[cmd->case_entry_count++] = entry;
        while (*pos < count &&
              (strcmp(tokens[*pos], ";") == 0 ||
               strcmp(tokens[*pos], ";;") == 0 ||
               strcmp(tokens[*pos], ")") == 0))
              (*pos)++;
    }
    if (*pos < count && strcmp(tokens[*pos], "esac") == 0) (*pos)++;
    return cmd;
}

static command_t *parse_simple(char **tokens, int *pos, int count) {
    if (strcmp(tokens[*pos], "(") == 0) return parse_subshell(tokens, pos, count);
    command_t *cmd = malloc(sizeof(command_t));
    memset(cmd, 0, sizeof(command_t));
    cmd->type = CMD_SIMPLE;
    cmd->args = malloc(sizeof(char*) * 64);
    cmd->arg_count = 0;
    while (*pos < count) {
        if (strcmp(tokens[*pos], "if") == 0 ||
            strcmp(tokens[*pos], "while") == 0 ||
            strcmp(tokens[*pos], "for") == 0 ||
            strcmp(tokens[*pos], "then") == 0 ||
            strcmp(tokens[*pos], "else") == 0 ||
            strcmp(tokens[*pos], "fi") == 0 ||
            strcmp(tokens[*pos], "do") == 0 ||
            strcmp(tokens[*pos], "done") == 0 ||
            strcmp(tokens[*pos], ";") == 0 ||
            strcmp(tokens[*pos], "|") == 0 ||
            strcmp(tokens[*pos], "&&") == 0 ||
            strcmp(tokens[*pos], "||") == 0)
            break;
        if (strcmp(tokens[*pos], "&") == 0) {
            cmd->background = 1;
            (*pos)++;
            continue;
        }
        if (strcmp(tokens[*pos], "<") == 0) {
            (*pos)++;
            if (*pos < count) { cmd->input_file = strdup(tokens[*pos]); (*pos)++; }
            continue;
        }
        if (strcmp(tokens[*pos], ">") == 0 || strcmp(tokens[*pos], ">>") == 0) {
            int is_append = (strcmp(tokens[*pos], ">>") == 0);
            (*pos)++;
            if (*pos < count) { cmd->output_file = strdup(tokens[*pos]); cmd->append_output = is_append; (*pos)++; }
            continue;
        }
        cmd->args[cmd->arg_count++] = strdup(tokens[*pos]);
        (*pos)++;
    }
    cmd->args[cmd->arg_count] = NULL;
    if (cmd->arg_count > 0) cmd->command = strdup(cmd->args[0]);
    return cmd;
}

static command_t *parse_subshell(char **tokens, int *pos, int count) {
    (*pos)++;
    char buffer[4096] = "";
    int nested = 1;
    while (*pos < count && nested > 0) {
        if (strcmp(tokens[*pos], "(") == 0) nested++;
        else if (strcmp(tokens[*pos], ")") == 0) {
            nested--;
            if(nested == 0) { (*pos)++; break; }
        }
        if (nested > 0) { strcat(buffer, tokens[*pos]); strcat(buffer, " "); }
        (*pos)++;
    }
    if (nested != 0) { fprintf(stderr, "Error: missing closing parenthesis\n"); return NULL; }
    command_t *cmd = malloc(sizeof(command_t));
    memset(cmd, 0, sizeof(command_t));
    cmd->type = CMD_SIMPLE;
    cmd->args = malloc(sizeof(char*) * 4);
    cmd->args[0] = strdup("sh");
    cmd->args[1] = strdup("-c");
    cmd->args[2] = strdup(buffer);
    cmd->args[3] = NULL;
    cmd->arg_count = 3;
    cmd->command = strdup(buffer);
    while (*pos < count &&
          (strcmp(tokens[*pos], "<") == 0 ||
           strcmp(tokens[*pos], ">") == 0 ||
           strcmp(tokens[*pos], ">>") == 0)) {
        if (strcmp(tokens[*pos], "<") == 0) {
            (*pos)++;
            if (*pos < count) { cmd->input_file = strdup(tokens[*pos]); (*pos)++; }
        } else {
            int append = (strcmp(tokens[*pos], ">>") == 0);
            (*pos)++;
            if (*pos < count) { cmd->output_file = strdup(tokens[*pos]); cmd->append_output = append; (*pos)++; }
        }
    }
    return cmd;
}

// [Restored] Function: parse_pipeline
static command_t *parse_pipeline(char **tokens, int *pos, int count) {
    command_t *head = parse_simple(tokens, pos, count);
    command_t *current = head;
    while (*pos < count && strcmp(tokens[*pos], "|") == 0) {
        (*pos)++;  // Skip the pipe token
        current->next = parse_simple(tokens, pos, count);
        current = current->next;
    }
    return head;
}
