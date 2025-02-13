#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <signal.h>
#include "shell.h"

// Add help command text
static const char *HELP_TEXT = 
    "\nJShell - A simple Unix shell implementation\n"
    "Author: Jalen Francis\n"
    "\nFeatures:\n"
    "- Command execution with arguments\n"
    "- Input/Output redirection (>, >>, <)\n"
    "- Pipeline commands (|)\n"
    "- Background processes (&)\n"
    "- Environment variables (export, unset)\n"
    "- History with up/down arrows\n"
	"- Cursor with left/right arrows\n"
    "- Tab completion\n"
    "\nBuilt-in commands:\n"
    "  help     - Show this help message\n"
    "  cd       - Change directory\n"
    "  exit     - Exit the shell\n"
    "  env      - Show environment variables\n"
    "  export   - Set environment variable\n"
    "  unset    - Unset environment variable\n"
    "  fg [job]  - Bring job to foreground\n"
    "  bg [job]  - Continue job in background\n"
    "  kill [job] - Terminate job\n"
    "\nKey bindings:\n"
    "  Ctrl+C   - Interrupt foreground process\n"
    "  Ctrl+Z   - Suspend foreground process\n"
    "  Up/Down  - Navigate command history\n"
	"  Left/Right- Navigate cursor\n"
    "  Tab      - Auto-complete commands and files\n\n";

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

void execute_command(command_t *cmd) {
    if (cmd->args[0] == NULL) {
        return;
    }

    // Handle built-in commands
    if (strcmp(cmd->args[0], "help") == 0) {
        printf("%s", HELP_TEXT);
        return;
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
        return;
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
        return;
    }

    // Add env command handling
    if (strcmp(cmd->args[0], "env") == 0) {
        extern char **environ;
        for (char **env = environ; *env; env++) {
            printf("%s\n", *env);
        }
        return;
    }

    if (strcmp(cmd->args[0], "cd") == 0) {
        if (cmd->args[1] == NULL) {
            fprintf(stderr, "cd: expected argument\n");
        } else {
            if (chdir(cmd->args[1]) != 0) {
                perror("cd: could not open directory");
            }
        }
        return;
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
        return;
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
        return;
    }

    // kill command
    if (strcmp(cmd->args[0], "kill") == 0) {
        if (!cmd->args[1]) {
            fprintf(stderr, "kill: usage: kill %%job_id\n");
            return;
        }
        int job_id = atoi(cmd->args[1] + 1);  // +1 to skip '%'
        kill_job(job_id);
        return;
    }

    // Handle pipe commands
    if (cmd->next != NULL) {
        execute_pipe(cmd, cmd->next);
        return;
    }

    pid_t pid = fork();
    if (pid == 0) {
        // Child process
        setpgid(0, 0);  // Create new process group
        
        // Reset all signal handlers to default
        signal(SIGINT, SIG_DFL);
        signal(SIGQUIT, SIG_DFL);
        signal(SIGTSTP, SIG_DFL);
        signal(SIGTTIN, SIG_DFL);
        signal(SIGTTOU, SIG_DFL);
        signal(SIGCHLD, SIG_DFL);

        // Handle input redirection
        if (cmd->input_file != NULL) {
            int fd_in = open(cmd->input_file, O_RDONLY);
            if (fd_in < 0) {
                perror("open input file");
                exit(EXIT_FAILURE);
            }
            dup2(fd_in, STDIN_FILENO);
            close(fd_in);
        }

        // Handle output redirection
        if (cmd->output_file != NULL) {
            int flags = O_CREAT | O_WRONLY;
            flags |= (cmd->append_output) ? O_APPEND : O_TRUNC;
            int fd_out = open(cmd->output_file, flags, 0644);
            if (fd_out < 0) {
                perror("open output file");
                exit(EXIT_FAILURE);
            }
            dup2(fd_out, STDOUT_FILENO);
            close(fd_out);
        }

        // Make sure output is line buffered
        setvbuf(stdout, NULL, _IOLBF, 0);
        setvbuf(stderr, NULL, _IOLBF, 0);

        if (execvp(cmd->args[0], cmd->args) == -1) {
            perror("execvp");
            exit(EXIT_FAILURE);
        }
    } else if (pid > 0) {
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
