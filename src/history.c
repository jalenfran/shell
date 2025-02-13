#include <string.h>
#include <stdlib.h>
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

void history_cleanup(void) {
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
