#ifndef SHELL_H
#define SHELL_H

#include "constants.h"

extern char current_command[MAX_CMD_LEN];

typedef struct command_t {
    char *command;
    char **args;
    int arg_count;
    char *input_file;
    char *output_file;
    int append_output;
    int background;
    struct command_t *next;
} command_t;

// Input handling
char *read_input(void);
command_t *parse_input(char *input);

// Command execution
void execute_command(command_t *cmd);
void execute_pipe(command_t *left, command_t *right);

// Background process handling
void add_background_process(pid_t pid);
void remove_background_process(pid_t pid);
int is_background_process(pid_t pid);
int get_job_number(pid_t pid);
char *get_process_command(pid_t pid);  // Make sure this is here

// Shell lifecycle
void shell_init(void);
void shell_loop(void);
void shell_cleanup(void);

// Utility functions
void command_free(command_t *cmd);
char **get_path_completions(const char *path, int *count);

// Job control functions
int get_pid_by_job_id(int job_id);
void continue_job(pid_t pid, int foreground);
void kill_job(int job_id);

// Add this declaration
void set_foreground_pid(pid_t pid);

#endif
