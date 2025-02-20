#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "command_registry.h"

#define MAX_COMMANDS 50

static command_entry_t *commands = NULL;
static int command_count = 0;

void init_command_registry(void) {
    commands = malloc(sizeof(command_entry_t) * MAX_COMMANDS);
    command_count = 0;
}

void cleanup_command_registry(void) {
    if (commands) {
        free(commands);
        commands = NULL;
    }
    command_count = 0;
}

int register_command(const char *name, command_func_t func, const char *help) {
    if (!commands || command_count >= MAX_COMMANDS)
        return -1;
    commands[command_count].name = name;
    commands[command_count].func = func;
    commands[command_count].help_text = help;
    command_count++;
    return 0;
}

const command_entry_t *lookup_command(const char *name) {
    for (int i = 0; i < command_count; i++) {
        if (strcmp(commands[i].name, name) == 0)
            return &commands[i];
    }
    return NULL;
}

void list_commands(void) {
    printf("Available commands:\n");
    for (int i = 0; i < command_count; i++) {
        printf("  %-15s - %s\n", commands[i].name, commands[i].help_text);
    }
}