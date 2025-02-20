#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include "shell.h"
#include "rc.h"
#include "alias.h"

// Returns the RC file path based on HOME.
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
        int len = eq - line - 7;  // "export " has length 7
        strncpy(var, line + 7, len);
        var[len] = '\0';
        char *value = eq + 1;
        if (*value == '\'' || *value == '"') {
            value++;
            char *end = strchr(value, *(eq - 1));
            if (end) *end = '\0';
        }
        setenv(var, value, 1);
    }
}

static void handle_alias(const char *line) {
    char *eq = strchr(line, '=');
    if (eq) {
        char name[256];
        int len = eq - line - 6;  // "alias " length 6
        strncpy(name, line + 6, len);
        name[len] = '\0';
        while (len > 0 && isspace(name[len - 1])) {
            name[--len] = '\0';
        }
        char *value = eq + 1;
        if (*value == '\'' || *value == '"') {
            char quote = *value;
            value++;
            size_t vlen = strlen(value);
            if (vlen > 0 && value[vlen - 1] == quote) {
                value[vlen - 1] = '\0';
            }
        }
        alias_add(name, value);
    }
}

int source_rc_file(const char *path) {
    if (!path || access(path, R_OK) != 0) {
        return -1;
    }
    FILE *rc = fopen(path, "r");
    if (!rc) {
        return -1;
    }
    char line[SHELL_MAX_INPUT];
    while (fgets(line, sizeof(line), rc)) {
        size_t len = strlen(line);
        while (len > 0 && (line[len - 1] == '\n' || isspace(line[len - 1]))) {
            line[--len] = '\0';
        }
        if (line[0] == '\0' || line[0] == '#') continue;
        if (strncmp(line, "echo", 4) == 0) continue;
        if (strncmp(line, "export ", 7) == 0) { handle_export(line); continue; }
        if (strncmp(line, "alias ", 6) == 0) { handle_alias(line); continue; }
    }
    fclose(rc);
    return 0;
}

void load_rc_file(void) {
    char *rc_path = get_rc_path();
    if (rc_path) {
        source_rc_file(rc_path);
    }
}
