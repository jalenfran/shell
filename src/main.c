#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

#define MAX_INPUT 1024
#define MAX_ARGS 64

void shell_loop();
char *read_input();
char **parse_input(char *input);
void execute_command(char **args);

int main() {
	shell_loop();
	return 0;
}

void shell_loop() {
	char *input;
	char **args;
	int status = 1;

	while (status) {
		printf("shell> ");
		input = read_input();
		args = parse_input(input);
		if (args[0] != NULL) {
			execute_command(args);
		}
		free(input);
		free(args);
	}
}

/*
 * Returns a character array that is stored on the heap.
 * The newline character is replaced with null terminating character.
 */
char *read_input(){
	char *input = malloc(MAX_INPUT);
	if (!input) {
		perror("malloc failed");
		exit(EXIT_FAILURE);
	}
	fgets(input, MAX_INPUT, stdin);
	input[strcspn(input, "\n")] = 0; // removes newline
	return input;
}

/*
 * Returns a 2D character array of max length MAX_ARGS
 * which contains all the tokens split up by spaces
 */
char **parse_input(char *input) {
	char **args = malloc(MAX_ARGS * sizeof(char *));
	if (!args) {
		perror("malloc failed");
		exit(EXIT_FAILURE);
	}
	char *token;
	int pos = 0;
	token = strtok(input, " ");
	while (token != NULL) {
		args[pos++] = token;
		token = strtok(NULL, " ");
	}
	args[pos] = NULL;
	return args;
}

void execute_command(char **args) {
	if (strcmp(args[0], "exit") == 0) {
		exit(0);
	}
	if (strcmp(args[0], "cd") == 0) {
		if (args[1]) {
			if (chdir(args[1]) != 0) {
				perror("cd failed");
			}
		} else {
			fprintf(stderr, "cd: expected argument\n");
		}
		return;
	}
	pid_t pid = fork();
	if (pid == 0) {
		// child process
		if (execvp(args[0], args) == -1) {
			perror("exec failed");
		}
		exit(EXIT_FAILURE);
	} else if (pid > 0) {
		// parent process
		wait(NULL);
	} else {
		perror("fork failed");
	}
}
