#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>
#include <dirent.h>
#include "shell.h"

#define MAX_HISTORY 100

static char *history_list[MAX_HISTORY];
static int history_count = 0;

/* Save a copy of the input to the history */
void add_history(const char *line) {
	if (history_count < MAX_HISTORY) {
		history_list[history_count++] = strdup(line);
	}
}

/* Find the first history entry that starts with the given prefix */
char *find_history_completion(const char *prefix) {
	size_t len = strlen(prefix);
	for (int i = 0; i < history_count; i++){
		if (strncmp(history_list[i], prefix, len) == 0) {
			return history_list[i];
		}
	}
	return NULL;
}

char *find_file_completion(const char *prefix) {
	DIR *d;
	struct dirent *dir;
	d = opendir(".");
	if (!d) return NULL;
	char *match = NULL;
	size_t prefix_len = strlen(prefix);
	while ((dir = readdir(d)) != NULL) {
		if (strncmp(dir->d_name, prefix, prefix_len) == 0) {
			match = strdup(dir->d_name);
			break;
		}
	}
	closedir(d);
	return match;
}

/*
 * Reads input one character at a time using system calls.
 * Supports:
 * 	- Arrow UpDown to cycle through previously entered commands
 * 	- Tab to complete the current input using history (if a match is found).
 */
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
	memset(buffer, 0, MAX_INPUT);
	int pos = 0;
	int c;
	int hist_index = history_count;

	while (1) {
		c = getchar();
		if (c == '\n') {
			putchar('\n');
			break;
		} else if (c == 127 || c == 8) { // backspace
			if (pos > 0) {
				pos--;
				buffer[pos] = '\0';
				printf("\b \b");
				fflush(stdout);
			}
		} else if (c == '\t') { // tab pressed
			buffer[pos] = '\0';
			char *completion = find_history_completion(buffer);
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

					// clear current input
					while (pos > 0) {
						printf("\b \b");
						pos--;
					}
					strcpy(buffer, completion);
					pos = strlen(buffer);
					printf("%s", buffer);
					fflush(stdout);
				} else {
					int token_start = pos;
					while (token_start > 0 && buffer[token_start -1] != ' '){
						token_start--;
					}
					while (pos > token_start) {
						printf("\b \b");
						pos--;
					}
					strcpy(buffer + token_start, completion);
					pos = token_start + strlen(completion);
					printf("%s", buffer + token_start);
					fflush(stdout);
					free(completion);
				}
			}
			// If no completion is found, ignore tab
		} else if (c == 27) { // handle escape sequences
			int next = getchar();
			if(next == '[') {
				int arrow = getchar();
				// up arrow
				if (arrow == 'A') {
					if (history_count > 0 && hist_index > 0) {
						hist_index--;
						while (pos > 0) {
							printf("\b \b");
							pos--;
						}
						strcpy(buffer, history_list[hist_index]);
						pos = strlen(buffer);
						printf("%s", buffer);
						fflush(stdout);
					}
				}
				// down arrow
				else if (arrow == 'B') {
					if (history_count > 0 && hist_index < history_count - 1) {
						hist_index++;
						while (pos > 0) {
							printf("\b \b");
							pos--;
						}
						strcpy(buffer, history_list[hist_index]);
						pos = strlen(buffer);
						printf("%s", buffer);
						fflush(stdout);
					} else {
						while (pos > 0) {
							printf("\b \b");
							pos--;
						}
						buffer[0] = '\0';
						hist_index = history_count;
					}
				}
			}
		} else {
			buffer[pos++] = c;
			buffer[pos] = '\0';
			putchar(c);
			fflush(stdout);
		}
	}
	tcsetattr(STDIN_FILENO, TCSANOW, &oldt);

	if(buffer[0] != '\0') {
		add_history(buffer);
	}
	
	return buffer;
}
