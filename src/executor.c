#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include "shell.h"

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
		if (execvp(args[0], args) == -1) {
			perror("exec failed");
			exit(EXIT_FAILURE);
		}
	} else if (pid > 0){
		wait(NULL);
	} else {
		perror("fork failed");
	}
}
