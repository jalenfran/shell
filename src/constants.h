#ifndef SHELL_CONSTANTS_H
#define SHELL_CONSTANTS_H

#include <limits.h>  // This will provide PATH_MAX

// Buffer sizes
#define SHELL_MAX_INPUT 4096
#define MAX_CMD_LEN 4096
#define MAX_PROMPT_LEN 4096
#define MAX_PATH_LEN PATH_MAX

// Array limits
#define MAX_ARGS 256
#define MAX_HISTORY 100
#define MAX_BACKGROUND_PROCESSES 64

// Shell prompt styling
#define GREEN_COLOR "\033[32m"
#define RESET_COLOR "\033[0m"
#define SHELL_PROMPT GREEN_COLOR "jshell>" RESET_COLOR " "

#endif
