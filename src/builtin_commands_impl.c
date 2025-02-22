#include "shell.h"
#include "command.h"
#include "alias.h"
#include "history.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include "job_manager.h"

// Add these external declarations at the top of the file
extern volatile int fg_wait;
extern volatile int fg_process_done;
extern volatile int print_prompt_pending;

// External helper functions used in executor.c
extern void list_jobs(void);
extern int get_job_number(pid_t pid);
extern pid_t get_pid_by_job_id(int job_id);
extern void continue_job(pid_t pid, int foreground);
extern void kill_job(int job_id);
extern char *get_process_command(pid_t pid);

// Minimal help text.
static const char *HELP_TEXT = 
    "\n\033[1;36m━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\033[0m\n"
    "\033[1;32mJShell v1.0\033[0m - A Modern Unix Shell Implementation\n"
    "Author: Jalen Francis\n"
    "\n\033[1;33mCore Features:\033[0m\n"
    "  • Command execution with argument handling\n"
    "  • Input/Output redirection using >, >>, and <\n"
    "  • Pipeline support using | operator\n"
    "  • Background process execution with &\n"
    "  • Environment variable management\n"
    "  • Command history and tab completion\n"
    "  • Alias management with validation\n"
    "  • Job control and process management\n"
    "\n\033[1;33mNew Language Constructs:\033[0m\n"
    "  • Conditional execution with if ... then ... else ... fi\n"
    "  • Switch-case statements using 'case ... in ... esac'\n"
    "  • Looping: for and while loops\n"
    "  • Subshell support using ( ... ) for grouping commands\n"
    "  • Logical operators: && to execute next command on success\n"
    "    and || to execute next command on failure\n"
    "\n\033[1;33mBuilt-in Commands:\033[0m\n"
    "  help       - Display this help message\n"
    "  cd         - Change current directory\n"
    "  exit       - Exit the shell\n"
    "  env        - Display all environment variables\n"
    "  export     - Set an environment variable (VAR=value)\n"
    "  unset      - Remove an environment variable\n"
    "  alias      - Create or list command aliases\n"
    "  unalias    - Remove an alias\n"
    "  jobs       - List all background jobs\n"
    "  fg/bg/kill - Job control commands\n"
    "  history    - Display command history\n"
    "\n\033[1;33mExamples:\033[0m\n"
    "  if true then echo yes else echo no fi\n"
    "  case $var in pattern1) cmd1 ;; *) cmd2 ;; esac\n"
    "  for i in 1 2 3 do echo $i done\n"
    "  (echo hello; echo world) | grep hello\n"
    "  cmd1 && cmd2 || cmd3\n"
    "\033[1;36m━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\033[0m\n";

// help command
int cmd_help(command_t * __attribute__((unused)) cmd) {
    printf("%s", HELP_TEXT);
    return 0;
}

// cd command
int cmd_cd(command_t *cmd) {
    if (cmd->args[1]) {
        char path[PATH_MAX];
        char oldpwd[PATH_MAX];

        // Save current working directory before changing directory.
        if (getcwd(oldpwd, sizeof(oldpwd)) == NULL) {
            perror("getcwd");
            return 1;
        }

        // Check for "cd -" behavior.
        if (strcmp(cmd->args[1], "-") == 0) {
            char *prev = getenv("OLDPWD");
            if (!prev) {
                fprintf(stderr, "cd: OLDPWD not set\n");
                return 1;
            }
            // Set the path to the previous directory.
            strncpy(path, prev, PATH_MAX);
            path[PATH_MAX - 1] = '\0';
            // Print the directory as per shell convention.
            printf("%s\n", path);
        }
        // Handle '~' expansion.
        else if (cmd->args[1][0] == '~') {
            char *home = getenv("HOME");
            if (!home) {
                fprintf(stderr, "cd: HOME environment variable not set\n");
                return 1;
            }
            if (strcmp(cmd->args[1], "~") == 0) {
                strncpy(path, home, PATH_MAX);
                path[PATH_MAX - 1] = '\0';
            } else {
                snprintf(path, PATH_MAX, "%s%s", home, cmd->args[1] + 1);
            }
        }
        // Otherwise, use the provided path.
        else {
            strncpy(path, cmd->args[1], PATH_MAX);
            path[PATH_MAX - 1] = '\0';
        }

        // Attempt to change directory.
        if (chdir(path) != 0)
            perror("cd");
        else {
            // On success, update OLDPWD to point to the previous directory.
            if (setenv("OLDPWD", oldpwd, 1) != 0) {
                perror("setenv");
            }
        }
    } else {
        fprintf(stderr, "cd: expected argument\n");
    }
    return 0;
}

// exit command
int cmd_exit(command_t * __attribute__((unused)) cmd) {
    exit(0);
    return 0;
}

// history command
int cmd_history(command_t *cmd) {
    int max_entries = history_size();
    if (cmd->args[1]) {
        char *endptr;
        long num = strtol(cmd->args[1], &endptr, 10);
        if (*endptr == '\0' && num > 0)
            max_entries = (int)num;
        else
            fprintf(stderr, "history: numeric argument required\n");
    }
    history_show(max_entries);
    return 0;
}

// alias command
int cmd_alias(command_t *cmd) {
    if (cmd->args[1] == NULL) {
        alias_list();
    } else {
        char *equals = strchr(cmd->args[1], '=');
        if (equals) {
            *equals = '\0';
            char *name = cmd->args[1];
            char *value = strdup(equals + 1);
            // Trim surrounding quotes if present.
            size_t vlen = strlen(value);
            if (vlen > 0 && (value[0] == '\'' || value[0] == '"')) {
                char quote = value[0];
                // Copy the null terminator as well.
                memmove(value, value + 1, strlen(value) + 1);
                vlen = strlen(value);
                if (vlen > 0 && value[vlen - 1] == quote)
                    value[vlen - 1] = '\0';
            }
            for (int i = 2; i < cmd->arg_count; i++) {
                size_t new_len = strlen(value) + 1 + strlen(cmd->args[i]) + 1;
                char *new_value = malloc(new_len);
                snprintf(new_value, new_len, "%s %s", value, cmd->args[i]);
            
                // Trim trailing quotes if any
                size_t vlen = strlen(new_value);
                if (vlen > 0 && (new_value[vlen - 1] == '\'' || new_value[vlen - 1] == '"')) {
                    new_value[vlen - 1] = '\0';
                }
            
                free(value);
                value = new_value;
            }
            char *temp = value;
            while (*temp && isspace(*temp)) temp++;
            char *final_value = strdup(temp);
            free(value);
            if (!*final_value) {
                fprintf(stderr, "alias: empty alias value not allowed\n");
                free(final_value);
                return 0;
            }
            alias_add(name, final_value);
            free(final_value);
        } else {
            char *alias_value = alias_get(cmd->args[1]);
            if (alias_value)
                printf("alias %s='%s'\n", cmd->args[1], alias_value);
            else
                fprintf(stderr, "alias: %s not found\n", cmd->args[1]);
        }
    }
    return 0;
}

// unalias command
int cmd_unalias(command_t *cmd) {
    if (cmd->args[1])
        alias_remove(cmd->args[1]);
    else
        fprintf(stderr, "unalias: missing argument\n");
    return 0;
}

// jobs command
int cmd_jobs(command_t * __attribute__((unused)) cmd) {
    list_jobs();
    return 0;
}

// fg command
int cmd_fg(command_t *cmd) {
    if (cmd->args[1]) {
        int job_id = atoi(cmd->args[1] + 1);  // skip '%'
        pid_t pid = get_pid_by_job_id(job_id);
        if (pid > 0) {
            fg_wait = 1;  // Set flag to prevent prompt
            fg_process_done = 0;
            print_prompt_pending = 0;  // Clear any pending prompts
            continue_job(pid, 1); // foreground
            fg_wait = 0;  // Clear flag after job completes
        } else {
            fprintf(stderr, "fg: no such job\n");
        }
    } else {
        fprintf(stderr, "fg: missing job specifier\n");
    }
    return 0;
}

// bg command
int cmd_bg(command_t *cmd) {
    if (cmd->args[1]) {
        int job_id = atoi(cmd->args[1] + 1);
        pid_t pid = get_pid_by_job_id(job_id);
        if (pid > 0) {
            continue_job(pid, 0);
            job_manager_update_state(pid, JOB_RUNNING);
            printf("[%d] %s &\n", job_id, get_process_command(pid));
        } else {
            fprintf(stderr, "bg: no such job\n");
        }
    } else {
        fprintf(stderr, "bg: missing job specifier\n");
    }
    return 0;
}

// kill command
int cmd_kill(command_t *cmd) {
    if (cmd->args[1]) {
        int job_id = atoi(cmd->args[1] + 1);
        kill_job(job_id);
    } else {
        fprintf(stderr, "kill: usage: kill %%job_id\n");
    }
    return 0;
}

// Updated export command
int cmd_export(command_t *cmd) {
    if (cmd->args[1]) {
        // Reconstruct the full assignment from tokens 1..(arg_count-1)
        int total_len = 0;
        for (int i = 1; i < cmd->arg_count; i++) {
            total_len += strlen(cmd->args[i]) + 1;  // plus space separator
        }
        char *assignment = malloc(total_len);
        assignment[0] = '\0';
        for (int i = 1; i < cmd->arg_count; i++) {
            strcat(assignment, cmd->args[i]);
            if (i < cmd->arg_count - 1)
                strcat(assignment, " ");
        }
        
        char *equals = strchr(assignment, '=');
        if (!equals) {
            fprintf(stderr, "export: invalid format (use VAR=value)\n");
            free(assignment);
            return 0;
        }
        
        *equals = '\0';
        char *var = assignment;
        char *value = equals + 1;
        
        // Remove surrounding quotes if present.
        size_t len = strlen(value);
        if (len >= 2 && ((value[0] == '\"' && value[len - 1] == '\"') ||
                         (value[0] == '\'' && value[len - 1] == '\''))) {
            value[len - 1] = '\0';  // remove closing quote
            value++;                // skip opening quote
        }
        
        if (setenv(var, value, 1) != 0)
            perror("export");
        
        free(assignment);
    } else {
        fprintf(stderr, "export: missing variable assignment\n");
    }
    return 0;
}

// unset command
int cmd_unset(command_t *cmd) {
    if (cmd->args[1]) {
        if (unsetenv(cmd->args[1]) != 0)
            perror("unset");
    } else {
        fprintf(stderr, "unset: missing variable name\n");
    }
    return 0;
}

// env command
int cmd_env(command_t * __attribute__((unused)) cmd) {
    extern char **environ;
    for (char **env = environ; *env; env++)
        printf("%s\n", *env);
    return 0;
}
