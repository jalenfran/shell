#define SHELL_MAX_INPUT 1024
#define MAX_ARGS 64

char *read_input();
char **parse_input(char *input);
void execute_command(char **args);
void shell_loop();
