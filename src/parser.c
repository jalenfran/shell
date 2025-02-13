#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "shell.h"

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
