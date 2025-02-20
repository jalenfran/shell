#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "history.h"
#include "constants.h"

static char *history_list[MAX_HISTORY];
static int history_count = 0;

void history_init(void) {
    memset(history_list, 0, sizeof(history_list));
    history_count = 0;
}

void history_add(const char *line) {
    if (history_count < MAX_HISTORY) {
        history_list[history_count++] = strdup(line);
    }
}

char *history_find_match(const char *prefix) {
    size_t len = strlen(prefix);
    for (int i = 0; i < history_count; i++) {
        if (strncmp(history_list[i], prefix, len) == 0) {
            return history_list[i];
        }
    }
    return NULL;
}

void history_save(void) {
    char *home = getenv("HOME");
    if (!home) return;

    char history_path[PATH_MAX];
    snprintf(history_path, sizeof(history_path), "%s/%s", home, HISTORY_FILE);
    
    FILE *fp = fopen(history_path, "w");
    if (!fp) return;

    for (int i = 0; i < history_count; i++) {
        if (history_list[i]) {
            fprintf(fp, "%s\n", history_list[i]);
        }
    }
    fclose(fp);
}

void history_load(void) {
    char *home = getenv("HOME");
    if (!home) return;

    char history_path[PATH_MAX];
    snprintf(history_path, sizeof(history_path), "%s/%s", home, HISTORY_FILE);
    
    FILE *fp = fopen(history_path, "r");
    if (!fp) return;

    char line[SHELL_MAX_INPUT];
    while (fgets(line, sizeof(line), fp)) {
        size_t len = strlen(line);
        if (len > 0 && line[len-1] == '\n') {
            line[len-1] = '\0';
        }
        history_add(line);
    }
    fclose(fp);
}

void history_cleanup(void) {
    history_save();
    for (int i = 0; i < history_count; i++) {
        free(history_list[i]);
    }
    history_count = 0;
}

char *history_get(int index) {
    if (index >= 0 && index < history_count) {
        return history_list[index];
    }
    return NULL;
}

int history_size(void) {
    return history_count;
}

void history_show(int max_entries) {
    int total = history_size();
    int start = (total > max_entries) ? (total - max_entries) : 0;
    
    for (int i = start; i < total; i++) {
        char *entry = history_get(i);
        if (entry) {
            printf("%5d  %s\n", i + 1, entry);
        }
    }
}
