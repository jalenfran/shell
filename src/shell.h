#ifndef SHELL_H
#define SHELL_H

#include "constants.h"
#include "rc.h"
#include "history.h"
#include "alias.h"
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

/**
 * Reads a line of input from the user.
 * @return Allocated string containing input, NULL on EOF/error
 * @pre None
 * @post Returns malloc'd string that caller must free
 */
char *read_input(void);

/**
 * Parses input string into command structure.
 * @param input The input string to parse
 * @return Parsed command structure or NULL on error
 * @pre input is a valid string
 * @post Returns malloc'd command structure that caller must free
 */
command_t *parse_input(char *input);

/**
 * Executes a single command.
 * @param cmd The command structure to execute
 * @pre cmd is a valid command structure
 * @post Command is executed and resources cleaned up
 */
void execute_command(command_t *cmd);

/**
 * Executes two commands connected by a pipe.
 * @param left Left side of pipe
 * @param right Right side of pipe
 * @pre Both commands are valid
 * @post Commands are executed with pipe between them
 */
void execute_pipe(command_t *left, command_t *right);

/**
 * Adds a process to background process list.
 * @param pid Process ID to add
 * @pre Valid process ID
 * @post Process is added to background list if space available
 */
void add_background_process(pid_t pid);

/**
 * Removes a process from background process list.
 * @param pid Process ID to remove
 * @pre Valid process ID
 * @post Process is removed from background list if present
 */
void remove_background_process(pid_t pid);

/**
 * Checks if a process is running in background.
 * @param pid Process ID to check
 * @return 1 if process is in background, 0 otherwise
 * @pre Valid process ID
 */
int is_background_process(pid_t pid);

/**
 * Sets the current foreground process.
 * @param pid Process ID to set as foreground
 * @pre Valid process ID
 * @post Process is set as current foreground process
 */
void set_foreground_pid(pid_t pid);

/**
 * Gets the command string associated with a process.
 * @param pid Process ID to query
 * @return Command string or NULL if not found
 * @pre Valid process ID
 */
char *get_process_command(pid_t pid);

/**
 * Gets job number for a process ID.
 * @param pid Process ID to query
 * @return Job number or -1 if not found
 * @pre Valid process ID
 */
int get_job_number(pid_t pid);

/**
 * Gets process ID for a job number.
 * @param job_id Job ID to query
 * @return Process ID or -1 if not found
 * @pre Valid job ID
 */
int get_pid_by_job_id(int job_id);

/**
 * Continues a stopped job.
 * @param pid Process ID to continue
 * @param foreground 1 for foreground, 0 for background
 * @pre Valid process ID
 * @post Job is continued in specified mode
 */
void continue_job(pid_t pid, int foreground);

/**
 * Terminates a job.
 * @param job_id Job ID to terminate
 * @pre Valid job ID
 * @post Job is terminated if it exists
 */
void kill_job(int job_id);

/**
 * Lists all current jobs.
 * @pre None
 * @post All jobs are printed to stdout
 */
void list_jobs(void);

// Shell lifecycle functions
void shell_init(void);
void shell_loop(void);
void shell_cleanup(void);

/**
 * Frees all resources used by a command.
 * @param cmd Command structure to free
 * @pre Valid command structure or NULL
 * @post All memory associated with cmd is freed
 */
void command_free(command_t *cmd);

/**
 * Gets possible path completions for tab completion.
 * @param path Partial path to complete
 * @param count Pointer to store number of completions
 * @return Array of possible completions, NULL on error
 * @pre path is non-NULL, count is valid pointer
 * @post count contains number of completions
 */
char **get_path_completions(const char *path, int *count);

/**
 * Executes a shell script.
 * @param filename Path to script file
 * @return 0 on success, -1 on failure
 * @pre filename is non-NULL and accessible
 * @post Script is executed if valid
 */
int execute_script(const char *filename);

/**
 * Checks if a file is a shell script.
 * @param filename Path to file
 * @return 1 if script, 0 otherwise
 * @pre filename is non-NULL
 */
int is_script_file(const char *filename);

/**
 * Displays command history entries.
 * @param max_entries Maximum number of entries to show
 * @pre max_entries > 0
 * @post Last max_entries commands are displayed
 */
void history_show(int max_entries);

extern int num_background_processes;
extern pid_t background_processes[];

#endif
