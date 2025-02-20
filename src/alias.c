#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "alias.h"

static alias_t *alias_list_head = NULL;

// Check alias name validity.
int is_valid_alias_name(const char *name) {
    if (!name || !*name) {
        return 0;
    }
    
    if (!isalpha(*name)) {
        return 0;
    }
    
    for (const char *p = name + 1; *p; p++) {
        if (!isalnum(*p) && *p != '_') {
            return 0;
        }
    }
    
    return 1;
}

void alias_init(void) {
    alias_list_head = NULL;
}

void alias_add(const char *name, const char *command) {
    alias_t *current = alias_list_head;
    
    while (current) {
        if (strcmp(current->name, name) == 0) {
            free(current->command);
            current->command = strdup(command);
            return;
        }
        current = current->next;
    }
    
    alias_t *new_alias = malloc(sizeof(alias_t));
    new_alias->name = strdup(name);
    new_alias->command = strdup(command);
    new_alias->next = alias_list_head;
    alias_list_head = new_alias;
}

void alias_remove(const char *name) {
    alias_t *current = alias_list_head;
    alias_t *prev = NULL;
    
    while (current) {
        if (strcmp(current->name, name) == 0) {
            if (prev) {
                prev->next = current->next;
            } else {
                alias_list_head = current->next;
            }
            free(current->name);
            free(current->command);
            free(current);
            return;
        }
        prev = current;
        current = current->next;
    }
}

char *alias_get(const char *name) {
    alias_t *current = alias_list_head;
    
    while (current) {
        if (strcmp(current->name, name) == 0) {
            return current->command;
        }
        current = current->next;
    }
    return NULL;
}

void alias_list(void) {
    alias_t *current = alias_list_head;
    
    while (current) {
        printf("alias %s='%s'\n", current->name, current->command);
        current = current->next;
    }
}

void alias_cleanup(void) {
    alias_t *current = alias_list_head;
    
    while (current) {
        alias_t *next = current->next;
        free(current->name);
        free(current->command);
        free(current);
        current = next;
    }
    alias_list_head = NULL;
}
