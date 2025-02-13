#ifndef SHELL_HISTORY_H
#define SHELL_HISTORY_H

void history_init(void);
void history_add(const char *line);
char *history_find_match(const char *prefix);
void history_cleanup(void);
char *history_get(int index);
int history_size(void);

#endif
