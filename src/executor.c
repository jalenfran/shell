#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <signal.h>
#include "shell.h"

// Help text for the shell
static const char *HELP_TEXT = 
    "\n\033[1;36m━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\033[0m\n"
    "\033[1;32mJShell v1.0\033[0m - A Modern Unix Shell Implementation\n"
    "Author: Jalen Francis\n"
    "\n\033[1;33mCore Features:\033[0m\n"
    "  • Command execution with argument handling\n"
    "  • Input/Output redirection using >, >>, and <\n"
    "  • Pipeline support using | operator\n"
    "  • Background process execution using &\n"
    "  • Environment variable management\n"
    "  • Command history navigation\n"
    "  • Tab completion for files and commands\n"
    "\n\033[1;33mBuilt-in Commands:\033[0m\n"
    "  \033[1mhelp\033[0m              Display this help message\n"
    "  \033[1mcd\033[0m [dir]          Change current directory\n"
    "  \033[1mexit\033[0m              Exit the shell\n"
    "  \033[1menv\033[0m               Display all environment variables\n"
    "  \033[1mexport\033[0m VAR=value  Set environment variable\n"
    "  \033[1munset\033[0m VAR         Remove environment variable\n"
    "\n\033[1;33mJob Control:\033[0m\n"
    "  \033[1mfg\033[0m [%job]         Bring job to foreground\n"
    "  \033[1mbg\033[0m [%job]         Continue job in background\n"
    "  \033[1mkill\033[0m %job         Terminate specified job\n"
    "\n\033[1;33mKey Bindings:\033[0m\n"
    "  \033[1m↑/↓\033[0m               Browse command history\n"
    "  \033[1m←/→\033[0m               Navigate cursor position\n"
    "  \033[1mTab\033[0m               Auto-complete commands and files\n"
    "  \033[1mCtrl+C\033[0m            Interrupt current process\n"
    "  \033[1mCtrl+Z\033[0m            Suspend current process\n"
    "\n\033[1;33mRedirection Examples:\033[0m\n"
    "  command > file     Write output to file\n"
    "  command >> file    Append output to file\n"
    "  command < file     Read input from file\n"
    "  cmd1 | cmd2        Pipe output of cmd1 to cmd2\n"
    "\n\033[1;33mBackground Job Examples:\033[0m\n"
    "  command &          Run in background\n"
    "  fg %1             Resume job 1 in foreground\n"
    "  bg %1             Resume job 1 in background\n"
    "\033[1;36m━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\033[0m\n";

/**
 * Executes a pipeline of two commands
 * Creates pipe and forks processes for each command
 */
void execute_pipe(command_t *left, command_t *right) {
    int fd[2];
    if (pipe(fd) == -1) {
        perror("pipe error");
        return;
    }

    pid_t pid1 = fork();
    if (pid1 < 0) {
        perror("pipe fork error left");
        return;
    }
    if (pid1 == 0) {
        close(fd[0]);
        dup2(fd[1], STDOUT_FILENO);
        close(fd[1]);
        if (execvp(left->args[0], left->args) == -1) {
            perror("execvp left pipe");
            exit(EXIT_FAILURE);
        }
    }

    pid_t pid2 = fork();
    if (pid2 < 0) {
        perror("pipe fork error right");
        return;
    }
    if (pid2 == 0) {
        close(fd[1]);
        dup2(fd[0], STDIN_FILENO);
        close(fd[0]);
        if (execvp(right->args[0], right->args) == -1) {
            perror("execvp right pipe");
            exit(EXIT_FAILURE);
        }
    }
    close(fd[0]);
    close(fd[1]);
    waitpid(pid1, NULL, 0);
    waitpid(pid2, NULL, 0);
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
        waitpid(pid, &status, WUNTRACED);
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
 * Handles execution of built-in commands
 * Returns 1 if command was handled, 0 otherwise
 */
static int handle_builtin_command(command_t *cmd) {
    if (strcmp(cmd->args[0], "help") == 0) {
        printf("%s", HELP_TEXT);
        return 1;
    }

    // Add export command handling
    if (strcmp(cmd->args[0], "export") == 0) {
        if (cmd->args[1] == NULL) {
            fprintf(stderr, "export: missing variable assignment\n");
        } else {
            char *equals = strchr(cmd->args[1], '=');
            if (equals) {
                *equals = '\0';  // Split string at '='
                if (setenv(cmd->args[1], equals + 1, 1) != 0) {
                    perror("export failed");
                }
            } else {
                fprintf(stderr, "export: invalid format (use VAR=value)\n");
            }
        }
        return 1;
    }

    // Add unset command handling
    if (strcmp(cmd->args[0], "unset") == 0) {
        if (cmd->args[1] == NULL) {
            fprintf(stderr, "unset: missing variable name\n");
        } else {
            if (unsetenv(cmd->args[1]) != 0) {
                perror("unset failed");
            }
        }
        return 1;
    }

    // Add env command handling
    if (strcmp(cmd->args[0], "env") == 0) {
        extern char **environ;
        for (char **env = environ; *env; env++) {
            printf("%s\n", *env);
        }
        return 1;
    }

    if (strcmp(cmd->args[0], "cd") == 0) {
        if (cmd->args[1] == NULL) {
            fprintf(stderr, "cd: expected argument\n");
        } else {
            if (chdir(cmd->args[1]) != 0) {
                perror("cd: could not open directory");
            }
        }
        return 1;
    }

    // fg command
    if (strcmp(cmd->args[0], "fg") == 0) {
        int job_id = cmd->args[1] ? atoi(cmd->args[1] + 1) : 1;  // +1 to skip '%'
        pid_t pid = get_pid_by_job_id(job_id);
        if (pid > 0) {
            continue_job(pid, 1);  // 1 means foreground
        } else {
            fprintf(stderr, "fg: no such job\n");
        }
        return 1;
    }

    // bg command
    if (strcmp(cmd->args[0], "bg") == 0) {
        int job_id = cmd->args[1] ? atoi(cmd->args[1] + 1) : 1;
        pid_t pid = get_pid_by_job_id(job_id);
        if (pid > 0) {
            continue_job(pid, 0);  // 0 means background
            printf("[%d] %s &\n", job_id, get_process_command(pid));
        } else {
            fprintf(stderr, "bg: no such job\n");
        }
        return 1;
    }

    // kill command
    if (strcmp(cmd->args[0], "kill") == 0) {
        if (!cmd->args[1]) {
            fprintf(stderr, "kill: usage: kill %%job_id\n");
            return 1;
        }
        int job_id = atoi(cmd->args[1] + 1);  // +1 to skip '%'
        kill_job(job_id);
        return 1;
    }

    return 0;
}

/**
 * Main command execution function
 * Handles built-ins, pipes, and external commands
 */
void execute_command(command_t *cmd) {
    if (!cmd->args[0]) return;

    // Try built-in commands first
    if (handle_builtin_command(cmd)) return;

    // Handle pipes
    if (cmd->next) {
        execute_pipe(cmd, cmd->next);
        return;
    }

    // Execute external command
    pid_t pid = fork();
    if (pid == 0) {
        // Child process setup
        setpgid(0, 0);  // Create new process group
        
        // Reset all signal handlers to default
        signal(SIGINT, SIG_DFL);
        signal(SIGQUIT, SIG_DFL);
        signal(SIGTSTP, SIG_DFL);
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
        // Parent process handling
        setpgid(pid, pid);  // Put child in its own process group

        // Save command name immediately after fork for all processes
        strncpy(current_command, cmd->args[0], MAX_CMD_LEN - 1);
        current_command[MAX_CMD_LEN - 1] = '\0';
        
        if (!cmd->background) {
            set_foreground_pid(pid);
            
            tcsetpgrp(STDIN_FILENO, pid);
            
            int status;
            waitpid(pid, &status, WUNTRACED);
            
            tcsetpgrp(STDIN_FILENO, getpgrp());
            set_foreground_pid(0);  // Clear foreground pid

            if (WIFSTOPPED(status)) {
                // Only add to background list if process was stopped
                add_background_process(pid);
                printf("\n[%d] Stopped %s\n", get_job_number(pid), current_command);
            }
        } else {
            add_background_process(pid);
            printf("[%d] %d\n", get_job_number(pid), pid);
        }
    } else {
        perror("fork error");
    }
}
