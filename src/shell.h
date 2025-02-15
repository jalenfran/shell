#ifndef SHELL_H
#define SHELL_H

#include "constants.h"
#include <sys/types.h>

// Current command being executed
extern char current_command[MAX_CMD_LEN];

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

// Input handling functions
char *read_input(void);
command_t *parse_input(char *input);

// Command execution functions
void execute_command(command_t *cmd);
void execute_pipe(command_t *left, command_t *right);

// Process management functions
void add_background_process(pid_t pid);
void remove_background_process(pid_t pid);
int is_background_process(pid_t pid);
void set_foreground_pid(pid_t pid);
char *get_process_command(pid_t pid);

// Job control functions
int get_job_number(pid_t pid);
int get_pid_by_job_id(int job_id);
void continue_job(pid_t pid, int foreground);
void kill_job(int job_id);

// Shell lifecycle functions
void shell_init(void);
void shell_loop(void);
void shell_cleanup(void);

// Memory management
void command_free(command_t *cmd);

// Tab completion
char **get_path_completions(const char *path, int *count);

// Alias management functions
void alias_init(void);
void alias_add(const char *name, const char *command);
void alias_remove(const char *name);
char *alias_get(const char *name);
void alias_list(void);
void alias_cleanup(void);

// Add this function declaration before #endif
void list_jobs(void);

// Alias recursion check
int is_recursive_alias(const char *cmd, int depth);

// Script execution functions
int execute_script(const char *filename);
int is_script_file(const char *filename);

// RC file handling
#define RC_FILE ".jshellrc"
void load_rc_file(void);

extern int num_background_processes;
extern pid_t background_processes[];

void history_show(int max_entries);
int history_size(void);

#endif
