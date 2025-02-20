#ifndef SHELL_CONSTANTS_H
#define SHELL_CONSTANTS_H

#include <limits.h>

#define SHELL_MAX_INPUT 4096
#define MAX_CMD_LEN 4096
#define MAX_PROMPT_LEN 4096
#define MAX_PATH_LEN PATH_MAX

#define MAX_ARGS 256
#define MAX_HISTORY 1000
#define MAX_BACKGROUND_PROCESSES 64

#define GREEN_COLOR "\033[32m"
#define RESET_COLOR "\033[0m"
#define SHELL_PROMPT GREEN_COLOR "jshell>" RESET_COLOR " "

#endif
