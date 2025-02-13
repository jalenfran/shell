#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>
#include <dirent.h>
#include <errno.h>
#include <ctype.h>
#include "shell.h"
#include "history.h"

extern char current_input_buffer[];
extern int current_input_length;
extern int current_cursor_pos;
extern char current_prompt[];

// Helper function to update buffer state
static void update_buffer_state(char *buffer, int pos, int cursor) {
    strncpy(current_input_buffer, buffer, pos);
    current_input_buffer[pos] = '\0';
    current_input_length = pos;
    current_cursor_pos = cursor;
}

static volatile int input_interrupted = 0;  // Add this global variable

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
                // Don't break on interrupt, just continue reading
                continue;
            }
            break;
        }

        if (c == '\n') {
            putchar('\n');
            update_buffer_state(buffer, 0, 0);  // Clear buffer state
            break;
        } else if (c == 127 || c == 8) { // backspace
            if (cursor > 0) {
                cursor--;
                memmove(buffer + cursor, buffer + cursor + 1, pos - cursor);
                pos--;
                printf("\033[D%s \033[%dD", buffer + cursor, pos - cursor + 1);
                fflush(stdout);
            }
        } else if (c == '\t') {
            buffer[pos] = '\0';
            char *completion = history_find_match(buffer);
            int file_completion_used = 0;
            
            if (!completion) {
                int token_start = pos;
                while (token_start > 0 && buffer[token_start - 1] != ' ') {
                    token_start--;
                }
                char *prefix = buffer + token_start;
                char *file_completion = find_file_completion(prefix);
                if (file_completion) {
                    completion = file_completion;
                    file_completion_used = 1;
                }
            }
            
            if (completion) {
                if (!file_completion_used) {
                    for (int i = 0; i < pos; i++) {
                        printf("\b \b");
                    }
                    strcpy(buffer, completion);
                    pos = strlen(buffer);
                    cursor = pos;
                    printf("%s", buffer);
                } else {
                    int token_start = pos;
                    while (token_start > 0 && buffer[token_start - 1] != ' ') {
                        token_start--;
                    }
                    for (int i = 0; i < pos - token_start; i++) {
                        printf("\b \b");
                    }
                    buffer[token_start] = '\0';
                    strcat(buffer, completion);
                    pos = strlen(buffer);
                    cursor = pos;
                    printf("%s", buffer + token_start);
                    free(completion);
                }
                fflush(stdout);
            }
        } else if (c == 27) { // handle escape sequences
            int next = getchar();
            if (next == '[') {
                int arrow = getchar();
                if (arrow == 'A' || arrow == 'B') { // up/down arrows
                    while (pos > 0) {
                        printf("\b \b");
                        pos--;
                    }
                    if (arrow == 'A' && hist_index > 0) {
                        hist_index--;
                        char *hist_entry = history_get(hist_index);
                        if (hist_entry) {
                            strcpy(buffer, hist_entry);
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
                } else if (arrow == 'D' && cursor > 0) { // left arrow
                    cursor--;
                    printf("\033[D");
                    fflush(stdout);
                } else if (arrow == 'C' && cursor < pos) { // right arrow
                    cursor++;
                    printf("\033[C");
                    fflush(stdout);
                }
            }
        } else if (!iscntrl(c)) { // Regular character input
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

        // Update state after each change
        update_buffer_state(buffer, pos, cursor);
    }

    // If input was interrupted, don't return partial input
    if (input_interrupted) {
        input_interrupted = 0;  // Reset flag
        buffer[0] = '\0';      // Clear buffer
        pos = 0;               // Reset position
    }

    // Ensure buffer is null-terminated
    buffer[pos] = '\0';
    
    // Update final state
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