#ifndef COMMAND_REGISTRY_H
#define COMMAND_REGISTRY_H

#include "shell.h"
#include "command.h"

// Command function type definition
typedef int (*command_func_t)(command_t *cmd);

// Structure to hold command information
typedef struct {
    const char *name;
    command_func_t func;
    const char *help_text;
} command_entry_t;

/**
 * Initializes the command registry system.
 * Must be called before any other command registry functions.
 * @pre None
 * @post Command registry is initialized and empty
 */
void init_command_registry(void);

/**
 * Registers a new built-in command.
 * @param name Name of the command (must be unique)
 * @param handler Function to handle the command execution
 * @param description Brief description of the command
 * @return 0 on success, -1 if command already exists or registry is full
 * @pre Command registry is initialized
 * @post Command is added to registry if successful
 */
int register_command(const char *name, command_func_t func, const char *help);

/**
 * Executes a built-in command if it exists in the registry.
 * @param cmd Command structure containing command name and arguments
 * @return 1 if command was found and executed, 0 if not found
 * @pre Command registry is initialized, cmd is not NULL
 * @post Command is executed if found in registry
 */
const command_entry_t *lookup_command(const char *name);

/**
 * Lists all available commands with their descriptions.
 * @pre Command registry is initialized
 * @post All registered commands and descriptions are printed to stdout
 */
void list_commands(void);

/**
 * Cleans up and frees all resources used by the command registry.
 * @pre Command registry is initialized
 * @post All registry resources are freed
 */
void cleanup_command_registry(void);

#endif // COMMAND_REGISTRY_H