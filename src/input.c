#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>
#include <dirent.h>
#include <errno.h>
#include <ctype.h>
#include <sys/stat.h>
#include "shell.h"
#include "history.h"

extern char current_input_buffer[];
extern int current_input_length;
extern int current_cursor_pos;
extern char current_prompt[];

// Helper function: simplified comment.
static void update_buffer_state(char *buffer, int pos, int cursor) {
    strncpy(current_input_buffer, buffer, pos);
    current_input_buffer[pos] = '\0';
    current_input_length = pos;
    current_cursor_pos = cursor;
}

char **get_path_completions(const char *path, int *count) {
    char dir_path[PATH_MAX] = ".";
    const char *search_prefix = path;
    
    const char *last_slash = strrchr(path, '/');
    if (last_slash) {
        size_t dir_len = last_slash - path;
        strncpy(dir_path, path, dir_len);
        dir_path[dir_len] = '\0';
        search_prefix = last_slash + 1;
    }

    DIR *dir = opendir(*dir_path ? dir_path : ".");
    if (!dir) return NULL;

    *count = 0;
    struct dirent *entry;
    size_t prefix_len = strlen(search_prefix);
    
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        if (strncmp(entry->d_name, search_prefix, prefix_len) == 0) {
            (*count)++;
        }
    }

    if (*count == 0) {
        closedir(dir);
        return NULL;
    }

    char **matches = malloc(sizeof(char *) * (*count));
    if (!matches) {
        closedir(dir);
        return NULL;
    }

    rewinddir(dir);
    int i = 0;
    while ((entry = readdir(dir)) != NULL && i < *count) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        
        if (strncmp(entry->d_name, search_prefix, prefix_len) == 0) {
            char check_path[PATH_MAX];
            size_t check_result = snprintf(check_path, sizeof(check_path), "%s/%s",
                    (*dir_path ? dir_path : "."), entry->d_name);
            if (check_result >= sizeof(check_path)) {
                continue;
            }
            
            struct stat st;
            if (stat(check_path, &st) == 0) {
                matches[i] = malloc(PATH_MAX);
                if (last_slash) {
                    char *base_path = strncpy(malloc(last_slash - path + 2), 
                                           path, last_slash - path + 1);
                    base_path[last_slash - path + 1] = '\0';
                    
                    if (S_ISDIR(st.st_mode)) {
                        snprintf(matches[i], PATH_MAX, "%s%s/", 
                                base_path, entry->d_name);
                    } else {
                        snprintf(matches[i], PATH_MAX, "%s%s", 
                                base_path, entry->d_name);
                    }
                    free(base_path);
                } else {
                    if (S_ISDIR(st.st_mode)) {
                        snprintf(matches[i], PATH_MAX, "%s/", entry->d_name);
                    } else {
                        snprintf(matches[i], PATH_MAX, "%s", entry->d_name);
                    }
                }
                i++;
            }
        }
    }
    closedir(dir);
    return matches;
}

static volatile int input_interrupted = 0;  // global flag

char *read_input() {
    struct termios oldt, newt;
    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;
    newt.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);

    char *buffer = malloc(SHELL_MAX_INPUT);
    if (!buffer) {
        perror("malloc failed");
        exit(EXIT_FAILURE);
    }
    memset(buffer, 0, SHELL_MAX_INPUT);
    int cursor = 0;
    int pos = 0;
    int c;
    int hist_index = history_size();

    while (1) {
        c = getchar();
        
        if (c == -1) {
            if (errno == EINTR) {
                continue;
            }
            break;
        }

        if (c == '\n') {
            putchar('\n');
            update_buffer_state(buffer, 0, 0);
            break;
        } else if (c == 127 || c == 8) {
            if (cursor > 0) {
                cursor--;
                memmove(buffer + cursor, buffer + cursor + 1, pos - cursor);
                pos--;
                printf("\033[D%s \033[%dD", buffer + cursor, pos - cursor + 1);
                fflush(stdout);
            }
        } else if (c == '\t') {
            buffer[pos] = '\0';
            
            int saved_cursor = cursor;
            
            int token_start = pos;
            while (token_start > 0 && !isspace(buffer[token_start - 1])) {
                token_start--;
            }
            
            char *current_token = buffer + token_start;
            int token_len = pos - token_start;
            
            int count;
            char **matches = get_path_completions(current_token, &count);
            
            if (matches && count > 0) {
                if (count == 1) {
                    while (cursor < pos) {
                        printf("\033[C");
                        cursor++;
                    }
                    
                    for (int i = 0; i < token_len; i++) {
                        printf("\b \b");
                        cursor--;
                    }
                    
                    int completion_len = strlen(matches[0]);
                    if (pos + completion_len - token_len >= SHELL_MAX_INPUT) {
                        completion_len = SHELL_MAX_INPUT - (pos - token_len);
                    }
                    
                    if (pos > token_start + token_len) {
                        memmove(buffer + token_start + completion_len,
                                buffer + token_start + token_len,
                                pos - (token_start + token_len));
                    }
                    
                    strncpy(buffer + token_start, matches[0], completion_len);
                    pos = token_start + completion_len;
                    cursor = pos;
                    
                    printf("%s", matches[0]);
                } else {
                    printf("\n");
                    for (int i = 0; i < count; i++) {
                        printf("%s  ", matches[i]);
                    }
                    printf("\n%s%s", current_prompt, buffer);
                    
                    if ((size_t)saved_cursor < strlen(buffer)) {
                        printf("\033[%luD", strlen(buffer) - saved_cursor);
                        cursor = saved_cursor;
                    }
                }
                
                for (int i = 0; i < count; i++) {
                    free(matches[i]);
                }
                free(matches);
            } else {
                char *hist_match = history_find_match(current_token);
                if (hist_match) {
                    while (cursor < pos) {
                        printf("\033[C");
                        cursor++;
                    }
                    while (pos > 0) {
                        printf("\b \b");
                        pos--;
                        cursor--;
                    }
                    
                    strncpy(buffer, hist_match, SHELL_MAX_INPUT - 1);
                    buffer[SHELL_MAX_INPUT - 1] = '\0';
                    pos = strlen(buffer);
                    cursor = pos;
                    
                    printf("%s", buffer);
                }
            }
            fflush(stdout);
        } else if (c == 27) {
            int next = getchar();
            if (next == '[') {
                int arrow = getchar();
                if (arrow == 'A' || arrow == 'B') {
                    while (cursor < pos) {
                        printf("\033[C");
                        cursor++;
                    }
                    while (cursor > 0) {
                        printf("\b \b");
                        cursor--;
                    }
                    printf("\033[K");
                    pos = 0;

                    if (arrow == 'A' && hist_index > 0) {
                        hist_index--;
                        char *hist_entry = history_get(hist_index);
                        if (hist_entry) {
                            strncpy(buffer, hist_entry, SHELL_MAX_INPUT - 1);
                            buffer[SHELL_MAX_INPUT - 1] = '\0';
                            pos = strlen(buffer);
                            cursor = pos;
                            printf("%s", buffer);
                        }
                    } else if (arrow == 'B') {
                        hist_index++;
                        if (hist_index < history_size()) {
                            char *hist_entry = history_get(hist_index);
                            if (hist_entry) {
                                strcpy(buffer, hist_entry);
                                pos = strlen(buffer);
                                cursor = pos;
                                printf("%s", buffer);
                            }
                        } else {
                            buffer[0] = '\0';
                            hist_index = history_size();
                        }
                    }
                    fflush(stdout);
                } else if (arrow == 'D' && cursor > 0) {
                    cursor--;
                    printf("\033[D");
                    fflush(stdout);
                } else if (arrow == 'C' && cursor < pos) {
                    cursor++;
                    printf("\033[C");
                    fflush(stdout);
                }
            }
        } else if (!iscntrl(c)) {
            if (cursor < pos) {
                memmove(buffer + cursor + 1, buffer + cursor, pos - cursor);
                buffer[cursor] = c;
                pos++;
                printf("%s", buffer + cursor);
                printf("\033[%dD", pos - cursor - 1);
                cursor++;
            } else {
                buffer[pos++] = c;
                buffer[pos] = '\0';
                putchar(c);
                cursor++;
            }
        }

        update_buffer_state(buffer, pos, cursor);
    }

    if (input_interrupted) {
        input_interrupted = 0;
        buffer[0] = '\0';
        pos = 0;
    }

    buffer[pos] = '\0';
    
    if (strlen(buffer) > 0) {
        history_add(buffer);
    }
    
    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
    return buffer;
}

char *find_file_completion(const char *prefix) {
    DIR *dir;
    struct dirent *entry;
    char *match = NULL;
    size_t prefix_len = strlen(prefix);

    dir = opendir(".");
    if (dir == NULL) {
        return NULL;
    }

    while ((entry = readdir(dir)) != NULL) {
        if (strncmp(entry->d_name, prefix, prefix_len) == 0) {
            if (match == NULL) {
                match = strdup(entry->d_name);
            } else {
                free(match);
                match = NULL;
                break;
            }
        }
    }

    closedir(dir);
    return match;
}