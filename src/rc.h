#ifndef RC_H
#define RC_H

#include <limits.h>

#define RC_FILE ".jshellrc"
void load_rc_file(void);
char *get_rc_path(void);
int source_rc_file(const char *path);

#endif
