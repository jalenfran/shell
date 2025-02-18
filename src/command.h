#ifndef COMMAND_H
#define COMMAND_H

typedef enum {
    CMD_SIMPLE,
    CMD_IF,
    CMD_WHILE,    // New: while loop
    CMD_FOR,      // New: for loop
    CMD_CASE      // NEW: case statement
} command_type_t;

// NEW: Structure to store a case entry.
typedef struct case_entry_t {
    char *pattern;         // The pattern string to match
    struct command_t *body; // Command body to execute if matched
} case_entry_t;

// Command structure to store parsed command information
typedef struct command_t {
    command_type_t type;    // Type of command
    
    // For simple commands
    char *command;          // The main command (for simple command)
    char **args;            // Array of command arguments
    int arg_count;          // Number of arguments
    char *input_file;       // Input redirection file
    char *output_file;      // Output redirection file
    int append_output;      // Flag for append mode (>>)
    int background;         // Flag for background execution
    struct command_t *next; // Next command in pipeline

    // New flag to indicate alias expansion has been performed.
    int alias_expanded;     

    // For if commands (when type == CMD_IF)
    char *if_condition;            // Condition expression as a string
    struct command_t *then_branch; // Command to execute if condition true
    struct command_t *else_branch; // Optional command if condition false

    // For while loops (when type == CMD_WHILE)
    char *while_condition;         // The while loop condition
    struct command_t *while_body;  // Command(s) to execute in loop

    // For for loops (when type == CMD_FOR)
    char *for_variable;            // Loop variable name
    char **for_list;               // Array of items to iterate over (NULL-terminated)
    struct command_t *for_body;    // Command(s) to execute for each item

    // NEW: For case commands.
    char *case_expression;       // The word being matched.
    case_entry_t **case_entries; // Array of pointers to case entries.
    int case_entry_count;        // Number of case entries.

} command_t;

#endif // COMMAND_H
