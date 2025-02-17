#ifndef COMMAND_H
#define COMMAND_H

typedef enum {
    CMD_SIMPLE,
    CMD_IF
} command_type_t;

// Command structure to store parsed command information
typedef struct command_t {
    command_type_t type;    // New: type of command (simple, if, etc.)
    
    // For simple commands
    char *command;          // The main command (for simple command)
    char **args;            // Array of command arguments
    int arg_count;          // Number of arguments
    char *input_file;       // Input redirection file
    char *output_file;      // Output redirection file
    int append_output;      // Flag for append mode (>>)
    int background;         // Flag for background execution
    struct command_t *next; // Next command in pipeline

    // For if command (when type == CMD_IF)
    char *if_condition;           // Condition expression as a string
    struct command_t *then_branch; // Command to execute if condition true
    struct command_t *else_branch; // Optional command if condition false
} command_t;

#endif // COMMAND_H
