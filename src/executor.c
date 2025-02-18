#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <signal.h>
#include <ctype.h>
#include "shell.h"
#include "alias.h"
#include "command_registry.h"
#include "job_manager.h"

// Forward declarations for missing functions:
static int evaluate_condition(const char *cond);
static void execute_if_block(command_t *cmd);
static void execute_while(command_t *cmd);
static void execute_for(command_t *cmd);
static char *trim_quotes(const char *str);
static command_t *merge_commands(command_t *old_cmd, command_t *new_cmd);

extern int num_background_processes;
extern pid_t background_processes[];

/**
 * Executes a pipeline of two commands
 * Creates pipe and forks processes for each command
 */
void execute_pipe(command_t *left, command_t *right) {
    command_t *left_cmd = left;
    command_t *right_cmd = right;
    int pipe_created = 0;
    int fd[2] = {-1, -1};
    
    // Check for aliases in both commands before creating pipe
    char *left_alias = alias_get(left->args[0]);
    char *right_alias = alias_get(right->args[0]);
    
    // Handle alias expansion
    if (left_alias) {
        char *expanded = malloc(SHELL_MAX_INPUT);
        if (expanded) {
            snprintf(expanded, SHELL_MAX_INPUT, "%s", left_alias);
            for (int i = 1; left->args[i]; i++) {
                strcat(expanded, " ");
                strcat(expanded, left->args[i]);
            }
            left_cmd = parse_input(expanded);
            free(expanded);
        }
    }
    
    if (right_alias) {
        char *expanded = malloc(SHELL_MAX_INPUT);
        if (expanded) {
            snprintf(expanded, SHELL_MAX_INPUT, "%s", right_alias);
            for (int i = 1; right->args[i]; i++) {
                strcat(expanded, " ");
                strcat(expanded, right->args[i]);
            }
            right_cmd = parse_input(expanded);
            if (right_cmd) {
                right_cmd->output_file = right->output_file ? strdup(right->output_file) : NULL;
                right_cmd->append_output = right->append_output;
            }
            free(expanded);
        }
    }

    // Create pipe
    if (pipe(fd) == -1) {
        perror("pipe error");
    } else {
        pipe_created = 1;
        
        // First process
        pid_t pid1 = fork();
        if (pid1 == 0) {
            close(fd[0]);
            dup2(fd[1], STDOUT_FILENO);
            close(fd[1]);
            execvp(left_cmd->args[0], left_cmd->args);
            perror("execvp left pipe");
            exit(EXIT_FAILURE);
        } else if (pid1 > 0) {
            // Second process
            pid_t pid2 = fork();
            if (pid2 == 0) {
                close(fd[1]);
                dup2(fd[0], STDIN_FILENO);
                close(fd[0]);
                
                // Handle output redirection
                if (right->output_file != NULL) {
                    int flags = O_CREAT | O_WRONLY;
                    flags |= (right->append_output) ? O_APPEND : O_TRUNC;
                    int fd_out = open(right->output_file, flags, 0644);
                    if (fd_out >= 0) {
                        dup2(fd_out, STDOUT_FILENO);
                        close(fd_out);
                    }
                }
                
                execvp(right_cmd->args[0], right_cmd->args);
                perror("execvp right pipe");
                exit(EXIT_FAILURE);
            } else if (pid2 > 0) {
                close(fd[0]);
                close(fd[1]);
                waitpid(pid1, NULL, 0);
                waitpid(pid2, NULL, 0);
            }
        }
    }

    // Cleanup
    if (left_alias && left_cmd != left) {
        command_free(left_cmd);
    }
    if (right_alias && right_cmd != right) {
        command_free(right_cmd);
    }
    if (pipe_created) {
        if (fd[0] != -1) close(fd[0]);
        if (fd[1] != -1) close(fd[1]);
    }
}

/**
 * Continues a stopped job
 * @param foreground: 1 for foreground, 0 for background
 */
void continue_job(pid_t pid, int foreground) {
    if (kill(-pid, SIGCONT) < 0) {
        perror("kill (SIGCONT)");
        return;
    }

    if (foreground) {
        tcsetpgrp(STDIN_FILENO, pid);
        int status;
        // Wait in a loop until the process terminates or stops again.
        while (1) {
            if (waitpid(pid, &status, WUNTRACED) < 0)
                break;
            if (WIFEXITED(status) || WIFSIGNALED(status) || WIFSTOPPED(status))
                break;
        }
        tcsetpgrp(STDIN_FILENO, getpgrp());
        if (WIFSTOPPED(status)) {
            const char *cmd_str = get_process_command(pid);
            printf("\n[%d] Stopped %s\n", get_job_number(pid), cmd_str);
        }
    }
}

void kill_job(int job_id) {
    pid_t pid = get_pid_by_job_id(job_id);
    if (pid > 0) {
        if (kill(-pid, SIGTERM) < 0) {
            perror("kill (SIGTERM)");
        }
    } else {
        fprintf(stderr, "Invalid job number: %d\n", job_id);
    }
}

/**
 * Handles checking for alias expansion
 * Returns 1 if command was handled, 0 otherwise
 */
static int check_alias_expansion(command_t *cmd) {

    // Check for alias expansion
    if (cmd->args[0]) {
        char *alias_cmd = alias_get(cmd->args[0]);
        if (alias_cmd && !is_recursive_alias(cmd->args[0], 0)) {
            // NEW: Trim matching quotes from alias_cmd.
            char *trimmed_alias = trim_quotes(alias_cmd);
            char *expanded = malloc(SHELL_MAX_INPUT);
            
            // First copy the alias command
            strncpy(expanded, trimmed_alias, SHELL_MAX_INPUT - 1);
            
            // Then append any additional arguments
            for (int i = 1; cmd->args[i]; i++) {
                strcat(expanded, " ");
                strcat(expanded, cmd->args[i]);
            }
            
            // Parse the expanded command
            command_t *new_cmd = parse_input(expanded);
            free(expanded);
            free(trimmed_alias);
            
            if (new_cmd) {
                // If original command had redirection, merge it with new command
                if (!new_cmd->output_file && cmd->output_file) {
                    new_cmd->output_file = strdup(cmd->output_file);
                    new_cmd->append_output = cmd->append_output;
                }
                if (!new_cmd->input_file && cmd->input_file) {
                    new_cmd->input_file = strdup(cmd->input_file);
                }
                
                // Copy background state
                new_cmd->background = cmd->background;
                
                execute_command(new_cmd);
                command_free(new_cmd);
                return 1;
            }
        }
    }

    return 0;
}

// NEW: Helper to remove matching leading/trailing quotes.
static char *trim_quotes(const char *str) {
    if (!str) return NULL;
    // Skip any leading quotes (single or double)
    while (*str == '"' || *str == '\'') {
        str++;
    }
    size_t len = strlen(str);
    // Remove trailing quotes (single or double)
    while (len > 0 && (str[len - 1] == '"' || str[len - 1] == '\'')) {
        len--;
    }
    return strndup(str, len);
}

// Move helper function definition before it's used
int is_recursive_alias(const char *cmd, int depth) {
    if (depth > 10) return 1; // Prevent deep recursion
    
    char *alias_cmd = alias_get(cmd);
    if (!alias_cmd) return 0;
    
    // Parse the alias command to get the first word
    char *temp = strdup(alias_cmd);
    char *first_word = strtok(temp, " \t\n");
    
    if (!first_word) {
        free(temp);
        return 0;
    }
    
    // Check if the first word matches the original command
    int is_recursive = (strcmp(cmd, first_word) == 0) || 
                      is_recursive_alias(first_word, depth + 1);
    
    free(temp);
    return is_recursive;
}

// Modify list_jobs() to use job_manager:
void list_jobs(void) {
    int count = 0;
    job_t *job_list = job_manager_get_all_jobs(&count);
    if (count == 0) {
        printf("No active jobs\n");
        return;
    }
    for (int i = 0; i < count; i++) {
        const char *state_str = (job_list[i].state == JOB_RUNNING) ? "Running" :
                                (job_list[i].state == JOB_STOPPED) ? "Stopped" : "Done";
        printf("[%d] %s\t%s (pid: %d)\n", job_list[i].job_id, state_str, job_list[i].command, job_list[i].pid);
    }
}

int is_script_file(const char *filename) {
    size_t len = strlen(filename);
    return len > 4 && strcmp(filename + len - 4, ".jsh") == 0;
}

int execute_script(const char *filename) {
    FILE *script = fopen(filename, "r");
    if (!script) {
        perror("Failed to open script");
        return -1;
    }

    char line[SHELL_MAX_INPUT];
    int line_num = 0;
    
    while (fgets(line, sizeof(line), script)) {
        line_num++;
        
        // Remove trailing newline
        size_t len = strlen(line);
        if (len > 0 && line[len-1] == '\n') {
            line[len-1] = '\0';
        }
        
        // Skip empty lines and comments
        if (line[0] == '\0' || line[0] == '#') {
            continue;
        }
        
        command_t *cmd = parse_input(line);
        if (cmd) {
            execute_command(cmd);
            command_free(cmd);
        } else {
            fprintf(stderr, "Script error at line %d: Failed to parse command\n", line_num);
        }
    }

    fclose(script);
    return 0;
}

// NEW: Minimal implementation for evaluate_condition (uses system())
static int evaluate_condition(const char *cond) {
    if (!cond || strlen(cond) == 0) return 0;
    char *trimmed = strdup(cond); // In practice, trim whitespace as needed
    int ret = system(trimmed);
    free(trimmed);
    // system() returns 0 if command succeeded
    return (ret == 0);
}

// NEW: Helper function to execute if-block commands.
static void execute_if_block(command_t *cmd) {
    if (evaluate_condition(cmd->if_condition))
        execute_command(cmd->then_branch);
    else if (cmd->else_branch)
        execute_command(cmd->else_branch);
}

static void execute_while(command_t *cmd) {
    while (evaluate_condition(cmd->while_condition)) {
        execute_command(cmd->while_body);
    }
}

static void execute_for(command_t *cmd) {
    for (int i = 0; cmd->for_list && cmd->for_list[i] != NULL; i++) {
        setenv(cmd->for_variable, cmd->for_list[i], 1);
        execute_command(cmd->for_body);
    }
}

static command_t *expand_alias_for_pipeline(command_t *cmd) {
    if (!cmd || !cmd->args[0])
        return cmd;
    // If already expanded, skip further work.
    if (cmd->alias_expanded)
        return cmd;
    char *alias_str = alias_get(cmd->args[0]);
    if (!alias_str || is_recursive_alias(cmd->args[0], 0))
        return cmd;
    char *trimmed = trim_quotes(alias_str);
    char buffer[SHELL_MAX_INPUT] = {0};
    snprintf(buffer, SHELL_MAX_INPUT, "%s", trimmed);
    free(trimmed);
    for (int i = 1; cmd->args[i]; i++) {
        strcat(buffer, " ");
        strcat(buffer, cmd->args[i]);
    }
    command_t *expanded_cmd = parse_input(buffer);
    if (expanded_cmd) {
        // Copy redirection and background settings from the original command.
        if (cmd->output_file) {
            if (expanded_cmd->output_file)
                free(expanded_cmd->output_file);
            expanded_cmd->output_file = strdup(cmd->output_file);
            expanded_cmd->append_output = cmd->append_output;
        }
        if (cmd->input_file) {
            if (expanded_cmd->input_file)
                free(expanded_cmd->input_file);
            expanded_cmd->input_file = strdup(cmd->input_file);
        }
        expanded_cmd->background = cmd->background;
        // Preserve the pipeline chain
        command_t *chain = cmd->next;  // Save remainder.
        cmd->next = NULL;              // Detach current node.
        cmd = merge_commands(cmd, expanded_cmd);
        cmd->next = chain;             // Restore the pipeline chain.
        cmd->alias_expanded = 1;
    }
    return cmd;
}

static void execute_pipeline(command_t *cmd) {
    // Update all pipeline stages with alias expansion.
    for (command_t **p = &cmd; *p != NULL; p = &((*p)->next)) {
        *p = expand_alias_for_pipeline(*p);
    }
    
    // Count commands in pipeline
    int n = 0;
    command_t *cur = cmd;
    while (cur) { n++; cur = cur->next; }
    if (n == 0) return;

    int pipes[n-1][2];
    for (int i = 0; i < n - 1; i++) {
        if (pipe(pipes[i]) == -1) {
            perror("pipe");
            return;
        }
    }
    
    pid_t pids[n];
    cur = cmd;
    for (int i = 0; i < n; i++) {
        pids[i] = fork();
        if (pids[i] < 0) {
            perror("fork");
            return;
        }
        else if (pids[i] == 0) {  // Child process
            // If not first command, redirect stdin to previous pipe's read end.
            if (i > 0) {
                dup2(pipes[i-1][0], STDIN_FILENO);
            }
            // If not last command, redirect stdout to current pipe's write end.
            if (i < n - 1) {
                dup2(pipes[i][1], STDOUT_FILENO);
            }
            // Close all pipe FDs in child.
            for (int j = 0; j < n - 1; j++) {
                close(pipes[j][0]);
                close(pipes[j][1]);
            }
            // Handle any redirection defined on the individual command.
            if (cur->input_file) {
                int fd_in = open(cur->input_file, O_RDONLY);
                if (fd_in < 0) { perror(cur->input_file); exit(EXIT_FAILURE); }
                dup2(fd_in, STDIN_FILENO);
                close(fd_in);
            }
            if (cur->output_file) {
                int flags = O_CREAT | O_WRONLY | (cur->append_output ? O_APPEND : O_TRUNC);
                int fd_out = open(cur->output_file, flags, 0644);
                if (fd_out < 0) { perror(cur->output_file); exit(EXIT_FAILURE); }
                dup2(fd_out, STDOUT_FILENO);
                close(fd_out);
            }
            execvp(cur->args[0], cur->args);
            perror("execvp pipeline");
            exit(EXIT_FAILURE);
        }
        cur = cur->next;
    }
    // Parent: close all pipe FDs.
    for (int i = 0; i < n - 1; i++) {
        close(pipes[i][0]);
        close(pipes[i][1]);
    }
    // Wait for all child processes.
    for (int i = 0; i < n; i++) {
        waitpid(pids[i], NULL, 0);
    }
}

/**
 * Main command execution function
 * Handles built-ins, pipes, and external commands
 */
void execute_command(command_t *cmd) {
    // Dispatch control structures first.
    if (cmd->type == CMD_IF) {
        execute_if_block(cmd);
        return;
    } else if (cmd->type == CMD_WHILE) {
        execute_while(cmd);
        return;
    } else if (cmd->type == CMD_FOR) {
        execute_for(cmd);
        return;
    }
    
    if (!cmd->args[0]) return;
    
    // Handle built-in commands
    const command_entry_t *entry = lookup_command(cmd->args[0]);
    if (entry) {
        entry->func(cmd);
        return;
    }
    // *** If there is a pipeline, bypass simple alias expansion ***
    if (cmd->next) {
        execute_pipeline(cmd);
        return;
    }
    // Otherwise, perform alias expansion for a simple command.
    if (check_alias_expansion(cmd)) return;
    
    pid_t pid = fork();
    if (pid == 0) {
        // Child process: set up new process group and reset signal handlers
        setpgid(0, 0);
        signal(SIGINT, SIG_DFL);
        signal(SIGQUIT, SIG_DFL);
        signal(SIGTSTP, SIG_DFL); // <-- Reset SIGTSTP to default in the child
        signal(SIGTTIN, SIG_DFL);
        signal(SIGTTOU, SIG_DFL);
        signal(SIGCHLD, SIG_DFL);

        // Handle output redirection
        if (cmd->output_file != NULL) {
            int flags = O_CREAT | O_WRONLY;
            flags |= (cmd->append_output) ? O_APPEND : O_TRUNC;
            int fd_out = open(cmd->output_file, flags, 0644);
            if (fd_out < 0) {
                perror(cmd->output_file);
                exit(EXIT_FAILURE);
            }
            dup2(fd_out, STDOUT_FILENO);
            close(fd_out);
        }

        // Handle input redirection
        if (cmd->input_file != NULL) {
            int fd_in = open(cmd->input_file, O_RDONLY);
            if (fd_in < 0) {
                perror(cmd->input_file);
                exit(EXIT_FAILURE);
            }
            dup2(fd_in, STDIN_FILENO);
            close(fd_in);
        }

        if (execvp(cmd->args[0], cmd->args) == -1) {
            perror("execvp");
            exit(EXIT_FAILURE);
        }
    } else if (pid > 0) {
        setpgid(pid, pid); // place child in its own group
        strncpy(current_command, cmd->args[0], MAX_CMD_LEN - 1);
        current_command[MAX_CMD_LEN - 1] = '\0';
        
        if (!cmd->background) {
            set_foreground_pid(pid);
            tcsetpgrp(STDIN_FILENO, pid);
            int status;
            waitpid(pid, &status, WUNTRACED);
            tcsetpgrp(STDIN_FILENO, getpgrp());
            set_foreground_pid(0);
            if (WIFSTOPPED(status)) {
                // Only add on suspend as a foreground (non‐background) job
                job_manager_add_job(pid, current_command, 0);
                job_manager_update_state(pid, JOB_STOPPED);
                printf("\n[%d] Suspended %s\n", get_job_number(pid), current_command);
            }
            // Do not print or add "Done" message for foreground jobs that exit normally.
        } else {
            // For background jobs, add immediately with background flag set
            job_manager_add_job(pid, current_command, 1);
            printf("[%d] %d\n", get_job_number(pid), pid);
        }
    } else {
        perror("fork error");
    }
}

// Update sigchld_handler to print termination messages only for background jobs:
void sigchld_handler(int __attribute__((unused)) sig) {
    pid_t pid;
    int status;
    while ((pid = waitpid(-1, &status, WNOHANG | WUNTRACED | WCONTINUED)) > 0) {
        job_t *job = job_manager_get_job_by_pid(pid);
        if (job) {
            if (WIFCONTINUED(status)) {
                job_manager_update_state(pid, JOB_RUNNING);
            } else if (WIFSTOPPED(status)) {
                job_manager_update_state(pid, JOB_STOPPED);
                printf("\r\033[K[%d] Suspended %s\n", job->job_id, job->command);
            } else if (WIFSIGNALED(status)) {
                if (job->background)
                    printf("\r\033[K[%d] Terminated %s\n", job->job_id, job->command);
                job_manager_remove_job(pid);
            } else if (WIFEXITED(status)) {
                if (job->background)
                    printf("\r\033[K[%d] Done %s\n", job->job_id, job->command);
                job_manager_remove_job(pid);
            }
        }
    }
}

static void free_command_fields(command_t *cmd) {
    if (!cmd) return;
    if (cmd->command) { free(cmd->command); cmd->command = NULL; }
    if (cmd->args) {
        for (int i = 0; i < cmd->arg_count; i++) {
            free(cmd->args[i]);
        }
        free(cmd->args); cmd->args = NULL;
    }
    if (cmd->input_file) { free(cmd->input_file); cmd->input_file = NULL; }
    if (cmd->output_file) { free(cmd->output_file); cmd->output_file = NULL; }
    if (cmd->if_condition) { free(cmd->if_condition); cmd->if_condition = NULL; }
    if (cmd->while_condition) { free(cmd->while_condition); cmd->while_condition = NULL; }
    if (cmd->for_variable) { free(cmd->for_variable); cmd->for_variable = NULL; }
    if (cmd->for_list) {
        for (int i = 0; cmd->for_list[i] != NULL; i++) {
            free(cmd->for_list[i]);
        }
        free(cmd->for_list); cmd->for_list = NULL;
    }
}

// The pipeline pointer (old_cmd->next) is preserved.
static command_t *merge_commands(command_t *old_cmd, command_t *new_cmd) {
    // Free old fields
    free_command_fields(old_cmd);
    // Transfer fields from new_cmd to old_cmd
    old_cmd->command = new_cmd->command; new_cmd->command = NULL;
    old_cmd->args = new_cmd->args;         new_cmd->args = NULL;
    old_cmd->arg_count = new_cmd->arg_count;
    old_cmd->input_file = new_cmd->input_file; new_cmd->input_file = NULL;
    old_cmd->output_file = new_cmd->output_file; new_cmd->output_file = NULL;
    old_cmd->append_output = new_cmd->append_output;
    old_cmd->background = new_cmd->background;
    // For control structures, one might also copy the branches if needed.
    // The 'next' pointer remains unchanged.
    free(new_cmd);  // Free the container (its fields have been transferred)
    return old_cmd;
}
