#ifndef RC_H
#define RC_H

#include <limits.h>

#define RC_FILE ".jshellrc"

/**
 * Loads and processes the shell's RC file.
 * @pre None
 * @post RC file is processed if it exists and is readable
 */
void load_rc_file(void);

/**
 * Gets the full path to the RC file.
 * @return Full path to RC file or NULL if HOME environment variable is not set
 * @pre None
 * @post Returns static buffer containing path - do not free
 */
char *get_rc_path(void);

/**
 * Sources and processes an RC file from the given path.
 * @param path Full path to the RC file
 * @return 0 on success, -1 on failure
 * @pre path is a valid string or NULL
 * @post RC file contents are processed if file exists and is readable
 */
int source_rc_file(const char *path);

#endif
