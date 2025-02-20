#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "shell.h"
#include "command.h"
#include <stdlib.h> // for getenv

// Updated tokenize: also split on '|', '<', '>' (handle ">>" as one token)
// Update tokenize function to recognize && and ||
static char **tokenize(const char *input, int *count) {
    char **tokens = malloc(sizeof(char*) * 256);
    *count = 0;
    const char *p = input;
    while (*p) {
        while (isspace(*p)) p++;
        if (!*p) break;
        // Check for control characters outside quotes
        if (*p == ';' || *p == '&' || *p == '|' || *p == '<' || *p == '>' || *p == '(' || *p == ')') {
            // Check for && and ||
            if ((*p == '&' && *(p+1) == '&') || 
                (*p == '|' && *(p+1) == '|')) {
                char token[3] = {*p, *p, '\0'};  // Fixed multi-char constant issue
                tokens[(*count)++] = strdup(token);
                p += 2;
            }
            // For ">>", check next character
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
            while (*p && *p != quote) {
                token[t++] = *p++;
            }
            if (*p == quote) p++;
        } else {
            while (*p && !isspace(*p) && *p != ';' && *p != '&' && *p != '|' && *p != '<' && *p != '>' && *p != '(' && *p != ')') {
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
static command_t *parse_while(char **tokens, int *pos, int count);
static command_t *parse_for(char **tokens, int *pos, int count);
static command_t *parse_case(char **tokens, int *pos, int count);
static command_t *parse_simple(char **tokens, int *pos, int count);
static command_t *parse_subshell(char **tokens, int *pos, int count);

// NEW: parse_pipeline() parses a series of simple commands split by '|'.
// This ensures that pipelines are grouped before applying sequence (';') operators.
static command_t *parse_pipeline(char **tokens, int *pos, int count) {
    // Parse first simple command.
    command_t *head = parse_simple(tokens, pos, count);
    command_t *current = head;
    // While a pipe is found, parse the next simple command and attach it.
    while (*pos < count && strcmp(tokens[*pos], "|") == 0) {
        (*pos)++; // Skip '|'
        current->next = parse_simple(tokens, pos, count);
        current = current->next;
    }
    return head;
}

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
// Update parse_command to handle && and ||
static command_t *parse_command(char **tokens, int *pos, int count) {
    if (*pos >= count) return NULL;
    // NEW: Check for control structures first.
    if (strcmp(tokens[*pos], "if") == 0)
        return parse_if(tokens, pos, count);
    if (strcmp(tokens[*pos], "while") == 0)
        return parse_while(tokens, pos, count);
    if (strcmp(tokens[*pos], "for") == 0)
        return parse_for(tokens, pos, count);
    if (strcmp(tokens[*pos], "case") == 0)
        return parse_case(tokens, pos, count);

    // Otherwise, build a pipeline (which calls parse_simple and handles subshells).
    command_t *cmd = parse_pipeline(tokens, pos, count);

    // Handle sequence with semicolon:
    if (*pos < count && strcmp(tokens[*pos], ";") == 0) {
        (*pos)++;  // Skip semicolon
        command_t *seq = malloc(sizeof(command_t));
        memset(seq, 0, sizeof(command_t));
        seq->type = CMD_SEQUENCE;
        seq->then_branch = cmd;
        seq->else_branch = parse_command(tokens, pos, count);
        return seq;
    }
    // Handle logical operators (&& / ||)
    else if (*pos < count && (strcmp(tokens[*pos], "&&") == 0 || strcmp(tokens[*pos], "||") == 0)) {
        command_t *logical_cmd = malloc(sizeof(command_t));
        memset(logical_cmd, 0, sizeof(command_t));
        logical_cmd->type = (strcmp(tokens[*pos], "&&") == 0) ? CMD_AND : CMD_OR;
        (*pos)++;  // Skip the operator
        logical_cmd->then_branch = cmd;
        logical_cmd->else_branch = parse_command(tokens, pos, count);
        return logical_cmd;
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
        // If the next token is "if", treat it as else-if block recursively.
        if (*pos < count && strcmp(tokens[*pos], "if") == 0) {
            cmd->else_branch = parse_if(tokens, pos, count);
        } else {
            cmd->else_branch = parse_command(tokens, pos, count);
        }
    }
    // Expect "fi"
    if (*pos < count && strcmp(tokens[*pos], "fi") == 0) {
        (*pos)++;
    }
    return cmd;
}

// New parse_while: Syntax: while condition do <body> done
static command_t *parse_while(char **tokens, int *pos, int count) {
    command_t *cmd = malloc(sizeof(command_t));
    memset(cmd, 0, sizeof(command_t));
    cmd->type = CMD_WHILE;
    (*pos)++; // skip "while"
    // Collect condition tokens until "do" is found
    char cond[1024] = {0};
    while (*pos < count && strcmp(tokens[*pos], "do") != 0) {
        strcat(cond, tokens[*pos]);
        strcat(cond, " ");
        (*pos)++;
    }
    cmd->while_condition = strdup(cond);
    if (*pos < count && strcmp(tokens[*pos], "do") == 0)
        (*pos)++; // skip "do"
    // Parse the loop body until "done"
    cmd->while_body = parse_command(tokens, pos, count);
    if (*pos < count && strcmp(tokens[*pos], "done") == 0)
        (*pos)++;
    return cmd;
}

// New parse_for: Syntax: for var in item1 item2 ... do <body> done
static command_t *parse_for(char **tokens, int *pos, int count) {
    command_t *cmd = malloc(sizeof(command_t));
    memset(cmd, 0, sizeof(command_t));
    cmd->type = CMD_FOR;
    (*pos)++; // skip "for"
    // Next token should be variable
    if (*pos < count) {
        cmd->for_variable = strdup(tokens[*pos]);
        (*pos)++;
    }
    // Expect "in"
    if (*pos < count && strcmp(tokens[*pos], "in") == 0)
        (*pos)++;
    // Collect list of items until "do" is encountered
    char **list = malloc(sizeof(char*) * 64);
    int list_count = 0;
    while (*pos < count && strcmp(tokens[*pos], "do") != 0) {
        list[list_count++] = strdup(tokens[*pos]);
        (*pos)++;
    }
    list[list_count] = NULL;
    cmd->for_list = list;
    if (*pos < count && strcmp(tokens[*pos], "do") == 0)
        (*pos)++; // skip "do"
    // Parse loop body until "done"
    cmd->for_body = parse_command(tokens, pos, count);
    if (*pos < count && strcmp(tokens[*pos], "done") == 0)
        (*pos)++;
    return cmd;
}

// NEW: Helper for parsing a case entry’s body without splitting on semicolons.
static command_t *parse_case_body_simple(char **tokens, int *pos, int count) {
    int start = *pos;
    // Collect tokens until we find two consecutive semicolons (";;") or "esac"
    while (*pos < count) {
        if (strcmp(tokens[*pos], "esac") == 0) break;
        if (strcmp(tokens[*pos], ";") == 0) {
            if (*pos + 1 < count && strcmp(tokens[*pos + 1], ";") == 0) {
                break;
            }
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
    // Build a simple command from the joined string.
    command_t *cmd = malloc(sizeof(command_t));
    memset(cmd, 0, sizeof(command_t));
    cmd->type = CMD_SIMPLE;
    cmd->command = strdup(body_str);
    // For simplicity, split on whitespace
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

// Updated parse_case: use the simple case body parser.
static command_t *parse_case(char **tokens, int *pos, int count) {
    command_t *cmd = malloc(sizeof(command_t));
    memset(cmd, 0, sizeof(command_t));
    cmd->type = CMD_CASE;
    
    // Skip "case"
    (*pos)++;
    
    // Next token: expression to match.
    if (*pos < count) {
        cmd->case_expression = expand_variables_in_token(tokens[*pos]);
        (*pos)++;
    }
    // Expect "in"
    if (*pos < count && strcmp(tokens[*pos], "in") == 0)
        (*pos)++;
    
    // Initialize case_entries array.
    cmd->case_entries = NULL;
    cmd->case_entry_count = 0;
    
    // Process each case entry until "esac" is found.
    while (*pos < count && strcmp(tokens[*pos], "esac") != 0) {
        // Skip stray ";;" delimiters.
        while (*pos < count && strcmp(tokens[*pos], ";;") == 0) {
            (*pos)++;
        }
        if (*pos >= count || strcmp(tokens[*pos], "esac") == 0)
            break;
        
        // Read pattern token.
        char *pattern = strdup(tokens[*pos]);
        size_t len = strlen(pattern);
        if (len > 0 && pattern[len-1] == ')') {
            pattern[len-1] = '\0';
        }
        (*pos)++; // Consume pattern token.
        if (*pos < count && strcmp(tokens[*pos], ")") == 0) {
            (*pos)++;
        }
        
        // Build the case entry body using the new helper.
        command_t *body = parse_case_body_simple(tokens, pos, count);
        
        // Store the entry.
        cmd->case_entries = realloc(cmd->case_entries, sizeof(case_entry_t*) * (cmd->case_entry_count + 1));
        case_entry_t *entry = malloc(sizeof(case_entry_t));
        entry->pattern = pattern;
        entry->body = body;
        cmd->case_entries[cmd->case_entry_count++] = entry;
        
        // Skip stray delimiters.
        while (*pos < count && (strcmp(tokens[*pos], ";") == 0 ||
                                  strcmp(tokens[*pos], ";;") == 0 ||
                                  strcmp(tokens[*pos], ")") == 0)) {
            (*pos)++;
        }
    }
    if (*pos < count && strcmp(tokens[*pos], "esac") == 0)
        (*pos)++;
    return cmd;
}

// Updated parse_simple: detect redirection tokens, background marker, and expand variables.
static command_t *parse_simple(char **tokens, int *pos, int count) {
    // Check if the next token begins a subshell
    if (strcmp(tokens[*pos], "(") == 0) {
        return parse_subshell(tokens, pos, count);
    }
    
    // ...existing code for parsing a simple command (arguments, redirections, etc.)...
    command_t *cmd = malloc(sizeof(command_t));
    memset(cmd, 0, sizeof(command_t));
    cmd->type = CMD_SIMPLE;
    
    // Allocate args array
    cmd->args = malloc(sizeof(char*) * 64);
    cmd->arg_count = 0;
    
    while (*pos < count) {
        // Stop if a control token is encountered (if, while, for, etc.)
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
        // Check for background token '&'
        if (strcmp(tokens[*pos], "&") == 0) {
            cmd->background = 1;
            (*pos)++;
            continue;
        }
        // Handle input redirection
        if (strcmp(tokens[*pos], "<") == 0) {
            (*pos)++;
            if (*pos < count) {
                cmd->input_file = strdup(tokens[*pos]);
                (*pos)++;
                continue;
            }
        }
        // Handle output redirection
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
        // Otherwise, treat token as argument (with variable expansion)
        char *expanded = expand_variables_in_token(tokens[*pos]);
        cmd->args[cmd->arg_count++] = expanded;
        (*pos)++;
    }
    cmd->args[cmd->arg_count] = NULL;
    if (cmd->arg_count > 0)
        cmd->command = strdup(cmd->args[0]);

    return cmd;
}

// Modify parse_subshell() to build a simple command invoking sh -c

static command_t *parse_subshell(char **tokens, int *pos, int count) {
    // Assume tokens[*pos] == "(".
    (*pos)++; // Skip "(".
    char buffer[4096] = "";
    int nested = 1;
    while (*pos < count && nested > 0) {
        if (strcmp(tokens[*pos], "(") == 0) { 
            nested++;
        } else if (strcmp(tokens[*pos], ")") == 0) { 
            nested--;
            if(nested==0) { (*pos)++; break; }
        }
        if (nested > 0) {
            strcat(buffer, tokens[*pos]);
            strcat(buffer, " ");
        }
        (*pos)++;
    }
    if (nested != 0) {
        fprintf(stderr, "Error: missing closing parenthesis\n");
        return NULL;
    }
    // Create a simple command that invokes sh -c with the collected subshell commands.
    command_t *cmd = malloc(sizeof(command_t));
    memset(cmd, 0, sizeof(command_t));
    // Set type to simple command so that pipelining and redirection work unchanged.
    cmd->type = CMD_SIMPLE;
    cmd->args = malloc(sizeof(char*) * 4);
    cmd->args[0] = strdup("sh");
    cmd->args[1] = strdup("-c");
    cmd->args[2] = strdup(buffer);
    cmd->args[3] = NULL;
    cmd->arg_count = 3;
    cmd->command = strdup(buffer);
    
    // Parse any redirections following the subshell.
    while (*pos < count &&
          (strcmp(tokens[*pos], "<") == 0 ||
           strcmp(tokens[*pos], ">") == 0 ||
           strcmp(tokens[*pos], ">>") == 0)) {
        if (strcmp(tokens[*pos], "<") == 0) {
            (*pos)++;
            if (*pos < count) {
                cmd->input_file = strdup(tokens[*pos]);
                (*pos)++;
            }
        } else {
            int append = (strcmp(tokens[*pos], ">>") == 0);
            (*pos)++;
            if (*pos < count) {
                cmd->output_file = strdup(tokens[*pos]);
                cmd->append_output = append;
                (*pos)++;
            }
        }
    }
    return cmd;
}
