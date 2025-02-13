#ifndef SHELL_CONSTANTS_H
#define SHELL_CONSTANTS_H

// Buffer sizes
#define SHELL_MAX_INPUT 1024
#define MAX_CMD_LEN 256
#define PATH_MAX 1024

// Array limits
#define MAX_ARGS 64
#define MAX_HISTORY 100
#define MAX_BACKGROUND_PROCESSES 64

// Shell prompt styling
#define GREEN_COLOR "\033[32m"
#define RESET_COLOR "\033[0m"
#define SHELL_PROMPT GREEN_COLOR "jshell>" RESET_COLOR " "

#endif
