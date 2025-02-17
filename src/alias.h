#ifndef SHELL_ALIAS_H
#define SHELL_ALIAS_H

#include "constants.h"

typedef struct alias_t {
    char *name;
    char *command;
    struct alias_t *next;
} alias_t;

/**
 * Initializes the alias system.
 * @pre None
 * @post Alias system is initialized and ready for use
 */
void alias_init(void);

/**
 * Adds a new alias or updates an existing one.
 * @param name The name of the alias
 * @param command The command string to associate with the alias
 * @pre name and command are valid non-NULL strings
 * @post New alias is added or existing alias is updated
 */
void alias_add(const char *name, const char *command);

/**
 * Removes an alias from the system.
 * @param name The name of the alias to remove
 * @pre name is a valid non-NULL string
 * @post If alias exists, it is removed from the system
 */
void alias_remove(const char *name);

/**
 * Retrieves the command associated with an alias.
 * @param name The name of the alias to look up
 * @return The command string if found, NULL if not found
 * @pre name is a valid non-NULL string
 */
char *alias_get(const char *name);

/**
 * Lists all currently defined aliases.
 * @pre Alias system is initialized
 * @post All aliases are printed to standard output
 */
void alias_list(void);

/**
 * Cleans up the alias system and frees all resources.
 * @pre Alias system is initialized
 * @post All allocated memory is freed
 */
void alias_cleanup(void);

/**
 * Checks if the given alias name is valid.
 * @param name The alias name to validate
 * @return 1 if valid, 0 if invalid
 * @pre name is a non-NULL string
 */
int is_valid_alias_name(const char *name);

/**
 * Checks if an alias would cause infinite recursion.
 * @param cmd Command to check
 * @param depth Current recursion depth
 * @return 1 if recursive, 0 if not
 * @pre cmd is a valid string
 */
int is_recursive_alias(const char *cmd, int depth);

#endif
