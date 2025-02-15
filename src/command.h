#ifndef COMMAND_H
#define COMMAND_H

// Command structure to store parsed command information
typedef struct command_t {
    char *command;           // The main command
    char **args;            // Array of command arguments
    int arg_count;          // Number of arguments
    char *input_file;       // Input redirection file
    char *output_file;      // Output redirection file
    int append_output;      // Flag for append mode (>>)
    int background;         // Flag for background execution
    struct command_t *next; // Next command in pipeline
} command_t;

#endif // COMMAND_H
