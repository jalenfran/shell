#ifndef SHELL_ALIAS_H
#define SHELL_ALIAS_H

#include "constants.h"

typedef struct alias_t {
    char *name;
    char *command;
    struct alias_t *next;
} alias_t;

void alias_init(void);
void alias_add(const char *name, const char *command);
void alias_remove(const char *name);
char *alias_get(const char *name);
void alias_list(void);
void alias_cleanup(void);

int is_valid_alias_name(const char *name);

#endif
