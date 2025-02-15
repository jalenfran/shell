#ifndef HISTORY_H
#define HISTORY_H

void history_init(void);
void history_add(const char *line);
char *history_find_match(const char *prefix);
void history_cleanup(void);
char *history_get(int index);
int history_size(void);

#define HISTORY_FILE ".jshell_history"
void history_save(void);
void history_load(void);

#endif
