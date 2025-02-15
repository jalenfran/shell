#ifndef HISTORY_H
#define HISTORY_H

/**
 * Initializes the command history system.
 * @pre None
 * @post History system is initialized and ready for use
 */
void history_init(void);

/**
 * Adds a command line to the history.
 * @param line The command line to add
 * @pre line is a valid non-NULL string
 * @post Command is added to history if not empty or duplicate
 */
void history_add(const char *line);

/**
 * Finds the most recent command that starts with the given prefix.
 * @param prefix The prefix to search for
 * @return Matching command if found, NULL if no match
 * @pre prefix is a non-NULL string
 */
char *history_find_match(const char *prefix);

/**
 * Cleans up the history system and frees resources.
 * @pre History system is initialized
 * @post All allocated memory is freed
 */
void history_cleanup(void);

/**
 * Retrieves a command from history by index.
 * @param index The index of the command to retrieve (1-based)
 * @return The command at the given index, NULL if index invalid
 * @pre History system is initialized
 */
char *history_get(int index);

/**
 * Returns the current size of the history.
 * @return Number of commands in history
 * @pre History system is initialized
 */
int history_size(void);

#define HISTORY_FILE ".jshell_history"

/**
 * Saves the command history to disk.
 * @pre History system is initialized
 * @post History is saved to HISTORY_FILE in user's home directory
 */
void history_save(void);

/**
 * Loads the command history from disk.
 * @pre History system is initialized
 * @post Previous history is loaded from HISTORY_FILE if it exists
 */
void history_load(void);

#endif
