#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
#include "shell.h"

void execute_pipe(char **left, char **right) {
	int fd[2];
	if (pipe(fd) == -1) {
		perror("pipe error");
		return;
	}

	pid_t pid1 = fork();
	if (pid1 < 0) {
		perror("pipe fork error left");
		return;
	}
	if (pid1 == 0) {
		// child for left command
		close(fd[0]);
		dup2(fd[1], STDOUT_FILENO);
		close(fd[1]);
		if (execvp(left[0], left) == -1) {
			perror("execvp left pipe");
			exit(EXIT_FAILURE);
		}
	} 

	pid_t pid2 = fork();
	if (pid2 < 0) {
		perror("pipe fork error right");
		return;
	}
	if (pid2 == 0) {
		close(fd[1]);
		dup2(fd[0], STDIN_FILENO);
		close(fd[0]);
		if (execvp(right[0], right) == -1) {
			perror("execvp right pipe");
			exit(EXIT_FAILURE);
		}
	}
	close(fd[0]);
	close(fd[1]);
	waitpid(pid1, NULL, 0);
	waitpid(pid2, NULL, 0);
}
		

void execute_command(char **args) {
	if (args[0] == NULL) {
		return;
	}

	if (strcmp(args[0], "exit") == 0) {
		exit(0);
	}
	if (strcmp(args[0], "cd") == 0) {
		if (args[1] == NULL) {
			fprintf(stderr, "cd: expected argument\n");
		} else {
			if (chdir(args[1]) != 0) {
				perror("cd: could not open directory");
			}
		}
		return;
	}

	int i = 0;
	int pipe_index = -1;
	while (args[i] != NULL) {
		if (strcmp(args[i], "|") == 0) {
			pipe_index = i;
			break;
		}
		i++;
	}

	if (pipe_index != -1) {
		// splits command into two
		char **left = args;
		left[pipe_index] = NULL;
		char **right = &args[pipe_index + 1];
		execute_pipe(left, right);
		return;
	}

	// check for redirection
	int in_index = -1, out_index = -1, app_index = -1;
	i = 0;
	while (args[i] != NULL) {
		if (strcmp(args[i], "<") == 0) {
			in_index = i;
		} else if (strcmp(args[i], ">>") == 0) {
			app_index = i;
		} else if (strcmp(args[i], ">") == 0) {
			out_index = i;
		}
		i++;
	}
	
	pid_t pid = fork();
	if (pid == 0) {
		if (in_index != -1 && args[in_index + 1] != NULL) {
			int fd_in = open(args[in_index+1], O_RDONLY);
			if (fd_in < 0) {
				perror("open input file");
				exit(EXIT_FAILURE);
			}
			dup2(fd_in, STDIN_FILENO);
			close(fd_in);
			args[in_index] = NULL;
		}
		if (app_index != -1 && args[app_index + 1] != NULL) {
		       int fd_app = open(args[app_index + 1], O_CREAT | O_WRONLY | O_APPEND, 0644);
		       if (fd_app < 0) {
			       perror("open output file for append");
			       exit(EXIT_FAILURE);
		       }
		       dup2(fd_app, STDOUT_FILENO);
		       close(fd_app);
		       args[app_index] = NULL;
		}

		if (out_index != -1 && args[out_index + 1] != NULL) {
			int fd_out = open(args[out_index + 1], O_CREAT | O_WRONLY | O_TRUNC, 0644);
			if (fd_out < 0) {
				perror("open output file");
				exit(EXIT_FAILURE);
			}
			dup2(fd_out, STDOUT_FILENO);
			close(fd_out);
			args[out_index] = NULL;
		}
		if (execvp(args[0], args) == -1) {
			perror("execvp");
		}
		exit(EXIT_FAILURE);
	} else if (pid > 0) {
		wait(NULL);
	} else {
		perror("fork error");
	}

}
