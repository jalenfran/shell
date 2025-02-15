#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "command_registry.h"

#define MAX_COMMANDS 32

static command_entry_t *commands = NULL;
static int command_count = 0;

void command_registry_init(void) {
    commands = malloc(sizeof(command_entry_t) * MAX_COMMANDS);
    command_count = 0;
}

int register_command(const char *name, command_func_t handler, const char *description) {
    if (command_count >= MAX_COMMANDS) {
        return 0;
    }

    for (int i = 0; i < command_count; i++) {
        if (strcmp(commands[i].name, name) == 0) {
            return 0; // Command already exists
        }
    }

    commands[command_count].name = strdup(name);
    commands[command_count].handler = handler;
    commands[command_count].description = strdup(description);
    command_count++;
    return 1;
}

int execute_builtin(command_t *cmd) {
    if (!cmd || !cmd->args[0]) {
        return 0;
    }

    for (int i = 0; i < command_count; i++) {
        if (strcmp(commands[i].name, cmd->args[0]) == 0) {
            return commands[i].handler(cmd);
        }
    }
    return 0;
}

void list_commands(void) {
    printf("\nAvailable built-in commands:\n");
    for (int i = 0; i < command_count; i++) {
        printf("  %-15s - %s\n", commands[i].name, commands[i].description);
    }
    printf("\n");
}

void command_registry_cleanup(void) {
    for (int i = 0; i < command_count; i++) {
        free((void *)commands[i].name);
        free((void *)commands[i].description);
    }
    free(commands);
    commands = NULL;
    command_count = 0;
}
