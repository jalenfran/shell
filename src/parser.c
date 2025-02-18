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
        if (*p == ';' || *p == '&' || *p == '|' || *p == '<' || *p == '>' || *p == '(' || *p == ')') {
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
    command_t *cmd = NULL;
    if (*pos >= count) return NULL;

    // Handle subshell if it starts with '('
    if (strcmp(tokens[*pos], "(") == 0) {
        (*pos)++;  // Skip opening parenthesis
        cmd = malloc(sizeof(command_t));
        memset(cmd, 0, sizeof(command_t));
        cmd->type = CMD_SUBSHELL;
        
        // Parse commands inside subshell until matching ')'
        int nested = 1;
        int start_pos = *pos;
        
        // Find matching closing parenthesis
        while (*pos < count && nested > 0) {
            if (strcmp(tokens[*pos], "(") == 0) {
                nested++;
            } else if (strcmp(tokens[*pos], ")") == 0) {
                nested--;
            }
            if (nested > 0) {
                (*pos)++;
            }
        }
        
        if (*pos >= count || nested > 0) {
            fprintf(stderr, "Error: missing closing parenthesis\n");
            command_free(cmd);
            return NULL;
        }
        
        // Parse the commands inside the subshell
        cmd->subshell_cmd = parse_command(tokens + start_pos, &(int){0}, *pos - start_pos);
        
        (*pos)++;  // Skip the closing parenthesis
        
        // Handle any redirections or background after the subshell
        while (*pos < count) {
            if (strcmp(tokens[*pos], ">") == 0) {
                (*pos)++;
                if (*pos < count) {
                    cmd->output_file = strdup(tokens[*pos]);
                    cmd->append_output = 0;
                    (*pos)++;
                }
            } else if (strcmp(tokens[*pos], ">>") == 0) {
                (*pos)++;
                if (*pos < count) {
                    cmd->output_file = strdup(tokens[*pos]);
                    cmd->append_output = 1;
                    (*pos)++;
                }
            } else if (strcmp(tokens[*pos], "<") == 0) {
                (*pos)++;
                if (*pos < count) {
                    cmd->input_file = strdup(tokens[*pos]);
                    (*pos)++;
                }
            } else if (strcmp(tokens[*pos], "&") == 0) {
                cmd->background = 1;
                (*pos)++;
            } else {
                break;
            }
        }
        
        // Handle pipeline
        if (*pos < count && strcmp(tokens[*pos], "|") == 0) {
            (*pos)++; // skip "|"
            cmd->next = parse_command(tokens, pos, count);
        }
        
        return cmd;
    }

    if (strcmp(tokens[*pos], "if") == 0) {
        cmd = parse_if(tokens, pos, count);
    } else if (strcmp(tokens[*pos], "while") == 0) {
        cmd = parse_while(tokens, pos, count);
    } else if (strcmp(tokens[*pos], "for") == 0) {
        cmd = parse_for(tokens, pos, count);
    }
    // NEW: Dispatch for "case"
    else if (strcmp(tokens[*pos], "case") == 0) {
        cmd = parse_case(tokens, pos, count);
    }
    else {
        cmd = parse_simple(tokens, pos, count);
    }
    // Handle pipelines
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

// Parse a case statement.
// Expected syntax:
//   case word in
//       pattern1) command1 ;; 
//       pattern2) command2 ;; 
//       ... 
//   esac
static command_t *parse_case(char **tokens, int *pos, int count) {
    command_t *cmd = malloc(sizeof(command_t));
    memset(cmd, 0, sizeof(command_t));
    cmd->type = CMD_CASE;
    
    // Skip "case"
    (*pos)++;
    
    // Next token is the expression to match.
    if (*pos < count) {
        cmd->case_expression = strdup(tokens[*pos]);
        (*pos)++;
    }
    // Expect "in"
    if (*pos < count && strcmp(tokens[*pos], "in") == 0)
        (*pos)++;
    
    // Initialize case_entries array (we'll realloc as needed)
    cmd->case_entries = NULL;
    cmd->case_entry_count = 0;
    
    // Parse each case entry until "esac" is encountered.
    while (*pos < count && strcmp(tokens[*pos], "esac") != 0) {
        // Skip over stray ";;" tokens.
        if (strcmp(tokens[*pos], ";;") == 0) {
            (*pos)++;
            continue;
        }
        // Read pattern token.
        char *pattern = strdup(tokens[*pos]);
        // Remove trailing ')' if it exists in the pattern.
        size_t len = strlen(pattern);
        if (len > 0 && pattern[len-1] == ')') {
            pattern[len-1] = '\0';
        }
        (*pos)++; // consume pattern token
        
        // If the next token is ")" (separating pattern and body), skip it.
        if (*pos < count && strcmp(tokens[*pos], ")") == 0) {
            (*pos)++;
        }
        
        // Parse the command body for this entry.
        command_t *body = NULL;
        // If the next token is ";;", the body is empty.
        if (*pos < count && strcmp(tokens[*pos], ";;") != 0) {
            body = parse_command(tokens, pos, count);
        }
        
        // Allocate or enlarge the array and store this entry.
        cmd->case_entries = realloc(cmd->case_entries, sizeof(case_entry_t*) * (cmd->case_entry_count + 1));
        case_entry_t *entry = malloc(sizeof(case_entry_t));
        entry->pattern = pattern;
        entry->body = body;
        cmd->case_entries[cmd->case_entry_count++] = entry;
        
        // Skip over any stray delimiter tokens (";" or ";;") or ")" tokens.
        while (*pos < count && (strcmp(tokens[*pos], ";") == 0 || strcmp(tokens[*pos], ")") == 0)) {
            (*pos)++;
        }
    }
    // Skip "esac"
    if (*pos < count && strcmp(tokens[*pos], "esac") == 0)
        (*pos)++;
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
            strcmp(tokens[*pos], "while") == 0 ||
            strcmp(tokens[*pos], "for") == 0 ||
            strcmp(tokens[*pos], "then") == 0 ||
            strcmp(tokens[*pos], "else") == 0 ||
            strcmp(tokens[*pos], "fi") == 0 ||
            strcmp(tokens[*pos], "do") == 0 ||
            strcmp(tokens[*pos], "done") == 0 ||
            strcmp(tokens[*pos], ";") == 0 ||   // <-- semicolon handled here
            strcmp(tokens[*pos], "|") == 0)
            break;
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
