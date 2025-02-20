#ifndef COMMAND_H
#define COMMAND_H

typedef enum {
    CMD_SIMPLE,
    CMD_IF,
    CMD_WHILE,
    CMD_FOR,
    CMD_CASE,
    CMD_SUBSHELL,
    CMD_AND,
    CMD_OR,
    CMD_SEQUENCE
} command_type_t;

typedef struct case_entry_t {
    char *pattern;
    struct command_t *body;
} case_entry_t;

typedef struct command_t {
    command_type_t type;
    char *command;
    char **args;
    int arg_count;
    char *input_file;
    char *output_file;
    int append_output;
    int background;
    int last_status;
    struct command_t *next;
    int alias_expanded;
    char *if_condition;
    struct command_t *then_branch;
    struct command_t *else_branch;
    char *while_condition;
    struct command_t *while_body;
    char *for_variable;
    char **for_list;
    struct command_t *for_body;
    char *case_expression;
    case_entry_t **case_entries;
    int case_entry_count;
    struct command_t *subshell_cmd;
} command_t;

void command_free(command_t *cmd);

#endif
