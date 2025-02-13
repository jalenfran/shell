#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include "shell.h"

int main() {
	shell_loop();
	return 0;
}

void shell_loop() {
	char *input;
	char **args;
	int status = 1;
	char cwd[1024];

	while (status) {
		if (getcwd(cwd, sizeof(cwd)) == NULL) {
			perror("getcwd error");
			strcpy(cwd, "unknown");
		}
		printf("%s jshell> ", cwd);
		fflush(stdout);
		input = read_input();
		args = parse_input(input);
		if (args[0] != NULL) {
			execute_command(args);
		}
		free(input);
		free(args);
	}
}
