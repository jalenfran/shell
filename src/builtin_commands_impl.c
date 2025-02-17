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

// External helper functions used in executor.c
extern void list_jobs(void);
extern int get_job_number(pid_t pid);
extern pid_t get_pid_by_job_id(int job_id);
extern void continue_job(pid_t pid, int foreground);
extern void kill_job(int job_id);
extern char *get_process_command(pid_t pid);

// Minimal help text as in executor.c
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

// help command
int cmd_help(command_t *cmd) {
    // Mimic executor.c's logic: print help text (or list registry commands)
    printf("%s", HELP_TEXT);
    return 0;
}

// cd command
int cmd_cd(command_t *cmd) {
    if (cmd->args[1])
        if (chdir(cmd->args[1]) != 0)
            perror("cd");
        else { /* success */ }
    else {
        fprintf(stderr, "cd: expected argument\n");
    }
    return 0;
}

// exit command
int cmd_exit(command_t *cmd) {
    exit(0);
    return 0;
}

// history command
int cmd_history(command_t *cmd) {
    int max_entries = 0;
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
            char *value = equals + 1;
            // Handle optional quotes
            if (*value == '\'' || *value == '"') {
                value++;
                size_t len = strlen(value);
                if (len > 0 && (value[len-1]=='\'' || value[len-1]=='"'))
                    value[len-1] = '\0';
            }
            // Strip leading whitespace
            while (*value && isspace(*value)) value++;
            if (!*value) {
                fprintf(stderr, "alias: empty alias value not allowed\n");
                return 0;
            }
            alias_add(name, value);
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
int cmd_jobs(command_t *cmd) {
    list_jobs();
    return 0;
}

// fg command
int cmd_fg(command_t *cmd) {
    if (cmd->args[1]) {
        int job_id = atoi(cmd->args[1] + 1);  // skip '%'
        pid_t pid = get_pid_by_job_id(job_id);
        if (pid > 0)
            continue_job(pid, 1); // foreground
        else
            fprintf(stderr, "fg: no such job\n");
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
            continue_job(pid, 0);          // Send SIGCONT in background
            job_manager_update_state(pid, JOB_RUNNING);  // Update state to running
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

// export command
int cmd_export(command_t *cmd) {
    if (cmd->args[1]) {
        char *equals = strchr(cmd->args[1], '=');
        if (equals) {
            *equals = '\0';
            if (setenv(cmd->args[1], equals + 1, 1) != 0)
                perror("export");
        } else {
            fprintf(stderr, "export: invalid format (use VAR=value)\n");
        }
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
int cmd_env(command_t *cmd) {
    extern char **environ;
    for (char **env = environ; *env; env++)
        printf("%s\n", *env);
    return 0;
}
