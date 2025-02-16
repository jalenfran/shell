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
            // Create a new command with the alias expansion
            char *expanded = malloc(SHELL_MAX_INPUT);
            
            // First copy the alias command
            strncpy(expanded, alias_cmd, SHELL_MAX_INPUT - 1);
            
            // Then append any additional arguments
            for (int i = 1; cmd->args[i]; i++) {
                strcat(expanded, " ");
                strcat(expanded, cmd->args[i]);
            }
            
            // Parse the expanded command
            command_t *new_cmd = parse_input(expanded);
            free(expanded);
            
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

/**
 * Main command execution function
 * Handles built-ins, pipes, and external commands
 */
void execute_command(command_t *cmd) {
    if (!cmd->args[0]) return;
    
    // Handle built-in commands and alias expansion
    const command_entry_t *entry = lookup_command(cmd->args[0]);
    if (entry) {
        entry->func(cmd);
        return;
    }
    if (check_alias_expansion(cmd)) return;
    if (cmd->next) {
        execute_pipe(cmd, cmd->next);
        return;
    }
    
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
