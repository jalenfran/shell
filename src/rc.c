#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include "shell.h"
#include "rc.h"
#include "alias.h"

// Add debug flag
#define RC_DEBUG 0

char *get_rc_path(void) {
    static char rc_path[PATH_MAX];
    char *home = getenv("HOME");
    
    if (home) {
        snprintf(rc_path, sizeof(rc_path), "%s/%s", home, RC_FILE);
        return rc_path;
    }
    return NULL;
}

static void handle_export(const char *line) {
    char *eq = strchr(line, '=');
    if (eq) {
        char var[256];
        int len = eq - line - 7;  // 7 is length of "export "
        strncpy(var, line + 7, len);
        var[len] = '\0';
        
        // Remove quotes if present
        char *value = eq + 1;
        if (*value == '\'' || *value == '"') {
            value++;
            char *end = strchr(value, *eq);
            if (end) *end = '\0';
        }
        
        setenv(var, value, 1);
    }
}

static void handle_alias(const char *line) {
    char *eq = strchr(line, '=');
    if (eq) {
        char name[256];
        int len = eq - line - 6;  // 6 is length of "alias "
        strncpy(name, line + 6, len);
        name[len] = '\0';
        
        // Remove trailing spaces from name
        while (len > 0 && isspace(name[len-1])) {
            name[--len] = '\0';
        }
        
        // Handle quoted value
        char *value = eq + 1;
        if (*value == '\'' || *value == '"') {
            value++;
            char *end = strrchr(value, *eq);
            if (end) *end = '\0';
        }
        
        alias_add(name, value);
    }
}

int source_rc_file(const char *path) {
    if (!path || access(path, R_OK) != 0) {
        if (RC_DEBUG) printf("Cannot access RC file: %s\n", path);
        return -1;
    }

    FILE *rc = fopen(path, "r");
    if (!rc) {
        if (RC_DEBUG) perror("Failed to open RC file");
        return -1;
    }

    char line[SHELL_MAX_INPUT];
    while (fgets(line, sizeof(line), rc)) {
        // Remove trailing newline and spaces
        size_t len = strlen(line);
        while (len > 0 && (line[len-1] == '\n' || isspace(line[len-1]))) {
            line[--len] = '\0';
        }

        // Skip empty lines and comments
        if (line[0] == '\0' || line[0] == '#') {
            continue;
        }

        // Skip echo commands and welcome messages
        if (strncmp(line, "echo", 4) == 0) {
            continue;
        }

        // Handle export statements
        if (strncmp(line, "export ", 7) == 0) {
            handle_export(line);
            continue;
        }

        // Handle alias definitions
        if (strncmp(line, "alias ", 6) == 0) {
            handle_alias(line);
            continue;
        }
    }

    fclose(rc);
    return 0;
}

void load_rc_file(void) {
    char *rc_path = get_rc_path();
    if (rc_path) {
        // Only load the global .jshellrc
        source_rc_file(rc_path);
    }
}
