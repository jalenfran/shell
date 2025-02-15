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
#include "history.h"
#include "job_observer.h"  // Add this include at the top

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

// Update function declarations to use command_t directly
static int cmd_help(command_t *cmd);
static int cmd_cd(command_t *cmd);
static int cmd_exit(command_t *cmd);
static int cmd_history(command_t *cmd);
static int cmd_alias(command_t *cmd);
static int cmd_unalias(command_t *cmd);
static int cmd_export(command_t *cmd);
static int cmd_unset(command_t *cmd);
static int cmd_env(command_t *cmd);
static int cmd_jobs(command_t *cmd);
static int cmd_fg(command_t *cmd);
static int cmd_bg(command_t *cmd);
static int cmd_kill(command_t *cmd);

// Replace handle_builtin_command with this simpler version
static int handle_builtin_command(struct command_t *cmd) {
    return execute_builtin(cmd);
}

// Add this function to register all built-in commands
void register_builtin_commands(void) {
    command_registry_init();
    
    register_command("help", cmd_help, "Display help information");
    register_command("cd", cmd_cd, "Change directory");
    register_command("exit", cmd_exit, "Exit the shell");
    register_command("history", cmd_history, "Show command history");
    register_command("alias", cmd_alias, "Create or list aliases");
    register_command("unalias", cmd_unalias, "Remove an alias");
    register_command("export", cmd_export, "Set environment variable");
    register_command("unset", cmd_unset, "Unset environment variable");
    register_command("env", cmd_env, "Display environment variables");
    register_command("jobs", cmd_jobs, "List background jobs");
    register_command("fg", cmd_fg, "Bring job to foreground");
    register_command("bg", cmd_bg, "Continue job in background");
    register_command("kill", cmd_kill, "Kill a job");
}

// Implement command handlers
static int cmd_help(command_t *cmd) {
    (void)cmd; // Unused parameter
    
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
        "  • Alias management with validation\n"
        "  • Job control and process management\n"
        "\n\033[1;33mBuilt-in Commands:\033[0m\n"
        "  \033[1mhelp\033[0m              Display this help message\n"
        "  \033[1mcd\033[0m [dir]          Change current directory\n"
        "  \033[1mexit\033[0m              Exit the shell\n"
        "  \033[1menv\033[0m               Display all environment variables\n"
        "  \033[1mexport\033[0m VAR=value  Set environment variable\n"
        "  \033[1munset\033[0m VAR         Remove environment variable\n"
        "  \033[1malias\033[0m [name='cmd'] Create/list command aliases\n"
        "  \033[1munalias\033[0m name      Remove an alias\n"
        "  \033[1mjobs\033[0m              List all background jobs\n"
        "  \033[1mhistory\033[0m           Show entire command history\n"
        "  \033[1mhistory\033[0m [n]       Show command history (last n entries)\n"
        "\n\033[1;33mJob Control:\033[0m\n"
        "  \033[1mfg\033[0m [%job]         Bring job to foreground\n"
        "  \033[1mbg\033[0m [%job]         Continue job in background\n"
        "  \033[1mkill\033[0m %job         Terminate specified job\n"
        "  \033[1mjobs\033[0m              List all background jobs\n"
        "\n\033[1;33mKey Bindings:\033[0m\n"
        "  \033[1m↑/↓\033[0m               Browse command history\n"
        "  \033[1m←/→\033[0m               Navigate cursor position\n"
        "  \033[1mTab\033[0m               Auto-complete commands and files\n"
        "  \033[1mCtrl+C\033[0m            Interrupt current process\n"
        "  \033[1mCtrl+Z\033[0m            Suspend current process\n"
        "\n\033[1;33mAlias Examples:\033[0m\n"
        "  alias ll='ls -l'    Create alias for long listing\n"
        "  alias              Show all defined aliases\n"
        "  unalias ll         Remove the ll alias\n"
        "\n\033[1;33mRedirection Examples:\033[0m\n"
        "  command > file     Write output to file\n"
        "  command >> file    Append output to file\n"
        "  command < file     Read input from file\n"
        "  cmd1 | cmd2        Pipe output of cmd1 to cmd2\n"
        "\n\033[1;33mBackground Job Examples:\033[0m\n"
        "  command &          Run in background\n"
        "  jobs              List all background jobs\n"
        "  fg %1             Resume job 1 in foreground\n"
        "  bg %1             Resume job 1 in background\n"
        "  kill %1           Terminate job 1\n"
        "\033[1;36m━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\033[0m\n";
    
    printf("%s", HELP_TEXT);
    list_commands();
    return 1;
}

static int cmd_cd(command_t *cmd) {
    if (cmd->args[1] == NULL) {
        fprintf(stderr, "cd: expected argument\n");
    } else if (chdir(cmd->args[1]) != 0) {
        perror("cd");
    }
    return 1;
}

static int cmd_exit(command_t *cmd) {
    (void)cmd; // Unused parameter
    cleanup_background_processes();
    command_registry_cleanup();
    exit(0);
    return 1;
}

static int cmd_history(command_t *cmd) {
    int max_entries = history_size();
    
    if (cmd->args[1]) {
        char *endptr;
        long num = strtol(cmd->args[1], &endptr, 10);
        if (*endptr == '\0' && num > 0) {
            max_entries = (int)num;
        } else {
            fprintf(stderr, "history: numeric argument required\n");
            return 1;
        }
    }
    history_show(max_entries);
    return 1;
}

static int cmd_alias(command_t *cmd) {
    if (cmd->args[1] == NULL) {
        alias_list();
        return 1;
    }
    
    char *equals = strchr(cmd->args[1], '=');
    if (equals) {
        *equals = '\0';
        char *name = cmd->args[1];
        char *value = equals + 1;
        
        if (!is_valid_alias_name(name)) {
            fprintf(stderr, "alias: invalid alias name: %s\n", name);
            return 1;
        }
        
        if (*value == '\'' || *value == '"') {
            value++;
            size_t len = strlen(value);
            if (len > 0 && (value[len-1] == '\'' || value[len-1] == '"')) {
                value[len-1] = '\0';
            }
        }
        
        while (*value && isspace(*value)) value++;
        char *end = value + strlen(value) - 1;
        while (end > value && isspace(*end)) *end-- = '\0';
        
        if (!*value) {
            fprintf(stderr, "alias: empty alias value not allowed\n");
            return 1;
        }
        
        alias_add(name, value);
    } else {
        char *alias_value = alias_get(cmd->args[1]);
        if (alias_value) {
            printf("alias %s='%s'\n", cmd->args[1], alias_value);
        } else {
            fprintf(stderr, "alias: %s not found\n", cmd->args[1]);
        }
    }
    return 1;
}

static int cmd_unalias(command_t *cmd) {
    if (!cmd->args[1]) {
        fprintf(stderr, "unalias: missing argument\n");
        return 1;
    }
    alias_remove(cmd->args[1]);
    return 1;
}

static int cmd_export(command_t *cmd) {
    if (!cmd->args[1]) {
        fprintf(stderr, "export: missing variable assignment\n");
        return 1;
    }
    
    char *equals = strchr(cmd->args[1], '=');
    if (equals) {
        *equals = '\0';
        if (setenv(cmd->args[1], equals + 1, 1) != 0) {
            perror("export failed");
        }
    } else {
        fprintf(stderr, "export: invalid format (use VAR=value)\n");
    }
    return 1;
}

static int cmd_unset(command_t *cmd) {
    if (!cmd->args[1]) {
        fprintf(stderr, "unset: missing variable name\n");
        return 1;
    }
    if (unsetenv(cmd->args[1]) != 0) {
        perror("unset failed");
    }
    return 1;
}

static int cmd_env(command_t *cmd) {
    (void)cmd;
    extern char **environ;
    for (char **env = environ; *env; env++) {
        printf("%s\n", *env);
    }
    return 1;
}

static int cmd_jobs(command_t *cmd) {
    (void)cmd;
    list_jobs();
    return 1;
}

static int cmd_fg(command_t *cmd) {
    int job_id = cmd->args[1] ? atoi(cmd->args[1] + 1) : 1;
    pid_t pid = get_pid_by_job_id(job_id);
    if (pid > 0) {
        continue_job(pid, 1);
    } else {
        fprintf(stderr, "fg: no such job\n");
    }
    return 1;
}

static int cmd_bg(command_t *cmd) {
    int job_id = cmd->args[1] ? atoi(cmd->args[1] + 1) : 1;
    pid_t pid = get_pid_by_job_id(job_id);
    if (pid > 0) {
        continue_job(pid, 0);
        printf("[%d] %s &\n", job_id, get_process_command(pid));
    } else {
        fprintf(stderr, "bg: no such job\n");
    }
    return 1;
}

static int cmd_kill(command_t *cmd) {
    if (!cmd->args[1]) {
        fprintf(stderr, "kill: usage: kill %%job_id\n");
        return 1;
    }
    int job_id = atoi(cmd->args[1] + 1);
    kill_job(job_id);
    return 1;
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

// Add this function implementation
void list_jobs(void) {
    for (int i = 0; i < num_background_processes; i++) {
        pid_t pid = background_processes[i];
        // Check if process is still running
        int status;
        pid_t result = waitpid(pid, &status, WNOHANG);
        
        if (result == 0) {
            // Process is still running
            printf("[%d] Running\t%s (pid: %d)\n", 
                   i + 1, get_process_command(pid), pid);
        } else if (WIFSTOPPED(status)) {
            // Process is stopped
            printf("[%d] Stopped\t%s (pid: %d)\n", 
                   i + 1, get_process_command(pid), pid);
        }
    }
    if (num_background_processes == 0) {
        printf("No active jobs\n");
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

    // Try to expand alias before executing
    char *alias_value = alias_get(cmd->args[0]);
    if (alias_value) {
        // Create expanded command string with original arguments
        char expanded[SHELL_MAX_INPUT] = "";
        strcat(expanded, alias_value);
        for (int i = 1; cmd->args[i]; i++) {
            strcat(expanded, " ");
            strcat(expanded, cmd->args[i]);
        }
        
        // Parse and execute the expanded command
        command_t *expanded_cmd = parse_input(expanded);
        if (expanded_cmd) {
            // Copy over I/O redirection and background settings
            expanded_cmd->input_file = cmd->input_file ? strdup(cmd->input_file) : NULL;
            expanded_cmd->output_file = cmd->output_file ? strdup(cmd->output_file) : NULL;
            expanded_cmd->append_output = cmd->append_output;
            expanded_cmd->background = cmd->background;
            
            // Execute the expanded command
            execute_command(expanded_cmd);
            command_free(expanded_cmd);
            return;
        }
    }

    // Try built-in commands second
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
        
        // Notify observers of job start
        job_observer_notify_started(pid, current_command);
        
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
                job_observer_notify_stopped(pid, current_command);
            } else {
                job_observer_notify_completed(pid, current_command, status);
            }
        } else {
            add_background_process(pid);
            printf("[%d] %d\n", get_job_number(pid), pid);
        }
    } else {
        perror("fork error");
    }
}

// Update the sigchld_handler to notify observers
void sigchld_handler(int __attribute__((unused)) sig) {
    pid_t pid;
    int status;
    
    while ((pid = waitpid(-1, &status, WNOHANG | WUNTRACED)) > 0) {
        if (WIFSTOPPED(status)) {
            if (!is_background_process(pid)) {
                add_background_process(pid);
                printf("\r\033[K[%d] Stopped %s\n", get_job_number(pid), current_command);
                job_observer_notify_stopped(pid, current_command);
            }
        } else if (WIFSIGNALED(status)) {
            printf("\r\033[KTerminated\n");
            if (is_background_process(pid)) {
                remove_background_process(pid);
                job_observer_notify_completed(pid, get_process_command(pid), status);
            }
        } else if (WIFEXITED(status)) {
            if (is_background_process(pid)) {
                printf("\r\033[K[%d] Done %s\n", get_job_number(pid), get_process_command(pid));
                remove_background_process(pid);
                job_observer_notify_completed(pid, get_process_command(pid), status);
            }
        }
    }
}
