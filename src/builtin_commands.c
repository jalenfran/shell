#include <stdio.h>
#include "command_registry.h"
#include "shell.h"

extern int cmd_help(command_t *cmd);
extern int cmd_cd(command_t *cmd);
extern int cmd_exit(command_t *cmd);
extern int cmd_history(command_t *cmd);
extern int cmd_alias(command_t *cmd);
extern int cmd_unalias(command_t *cmd);
extern int cmd_jobs(command_t *cmd);
extern int cmd_fg(command_t *cmd);
extern int cmd_bg(command_t *cmd);
extern int cmd_kill(command_t *cmd);
extern int cmd_export(command_t *cmd);
extern int cmd_unset(command_t *cmd);
extern int cmd_env(command_t *cmd);

void register_builtin_commands(void) {
    register_command("help",   cmd_help,    "Display help message");
    register_command("cd",     cmd_cd,      "Change current directory");
    register_command("exit",   cmd_exit,    "Exit the shell");
    register_command("history",cmd_history, "Show command history");
    register_command("alias",  cmd_alias,   "Manage command aliases");
    register_command("unalias",cmd_unalias, "Remove a command alias");
    register_command("jobs",   cmd_jobs,    "List background jobs");
    register_command("fg",     cmd_fg,      "Bring job to foreground");
    register_command("bg",     cmd_bg,      "Continue job in background");
    register_command("kill",   cmd_kill,    "Terminate a job");
    register_command("export", cmd_export,  "Set environment variable");
    register_command("unset",  cmd_unset,   "Remove environment variable");
    register_command("env",    cmd_env,     "Display environment variables");
}
