#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <termios.h>
#include "shell.h"
#include "history.h"

// Add signal handler declarations at the top before any functions
static void sigchld_handler(int);
static void sigint_handler(int);
static void sigtstp_handler(int);
static void sigterm_handler(int);

// Change these from static to global
int current_cursor_pos = 0;
char current_prompt[SHELL_MAX_INPUT];

pid_t background_processes[MAX_BACKGROUND_PROCESSES];
int num_background_processes = 0;

// Define the global current_command variable (matches the extern declaration)
char current_command[MAX_CMD_LEN];

// Define these as global variables for input.c to use
char current_input_buffer[SHELL_MAX_INPUT];
int current_input_length = 0;

// Add this global variable
// static int current_cursor_pos = 0;

// Add a global flag for exit state
static volatile int exiting = 0;

// Add a global flag to indicate waiting for input
static volatile int in_input = 0;

// Add global variable to track foreground process
static pid_t foreground_pid = 0;

struct bg_process {
    pid_t pid;
    char command[MAX_CMD_LEN];
};

static struct bg_process bg_processes[MAX_BACKGROUND_PROCESSES];

void add_background_process(pid_t pid) {
    if (num_background_processes < MAX_BACKGROUND_PROCESSES) {
        bg_processes[num_background_processes].pid = pid;
        strncpy(bg_processes[num_background_processes].command, 
                current_command, 
                MAX_CMD_LEN - 1);
        background_processes[num_background_processes++] = pid;
    }
}

char *get_process_command(pid_t pid) {
    for (int i = 0; i < num_background_processes; i++) {
        if (bg_processes[i].pid == pid) {
            return bg_processes[i].command;
        }
    }
    return "unknown";
}

int is_background_process(pid_t pid) {
    for (int i = 0; i < num_background_processes; i++) {
        if (background_processes[i] == pid) {
            return 1;
        }
    }
    return 0;
}

void remove_background_process(pid_t pid) {
    for (int i = 0; i < num_background_processes; i++) {
        if (background_processes[i] == pid) {
            for (int j = i; j < num_background_processes - 1; j++) {
                background_processes[j] = background_processes[j + 1];
                bg_processes[j] = bg_processes[j + 1];
            }
            num_background_processes--;
            break;
        }
    }
}

int get_job_number(pid_t pid) {
    for (int i = 0; i < num_background_processes; i++) {
        if (background_processes[i] == pid) {
            return i + 1;  // Return 1-based job number
        }
    }
    return 1;  // Default to 1 if not found
}

int get_pid_by_job_id(int job_id) {
    job_id--; // Convert from 1-based to 0-based index
    if (job_id >= 0 && job_id < num_background_processes) {
        return background_processes[job_id];
    }
    return -1;
}

void print_prompt(void) {
    char cwd[PATH_MAX];
    char current_prompt[MAX_PROMPT_LEN];
    
    if (getcwd(cwd, sizeof(cwd)) == NULL) {
        strcpy(cwd, "???");
    }
    
    size_t prompt_result = snprintf(current_prompt, sizeof(current_prompt), "%s %s", cwd, SHELL_PROMPT);
    if (prompt_result >= sizeof(current_prompt)) {
        // Prompt too long, use fallback
        printf("%s ", SHELL_PROMPT);
        return;
    }
    
    printf("%s ", current_prompt);
}

static void set_signal_handlers() {
    struct sigaction sa;
    sigemptyset(&sa.sa_mask);
    
    // Disable SA_RESTART for SIGCHLD so that blocking input is interrupted
    sa.sa_flags = 0;
    sa.sa_handler = sigchld_handler;
    sigaction(SIGCHLD, &sa, NULL);
    
    // For SIGINT, keep SA_RESTART (or disable it if you want similar behavior)
    sa.sa_flags = 0;  // Optionally disable SA_RESTART here, too.
    sa.sa_handler = sigint_handler;
    sigaction(SIGINT, &sa, NULL);
    
    // For SIGTSTP, keep SA_RESTART disabled as before
    sa.sa_flags = 0;
    sa.sa_handler = sigtstp_handler;
    sigaction(SIGTSTP, &sa, NULL);
    
    // Ignore other signals as before
    signal(SIGQUIT, SIG_IGN);
    signal(SIGTTIN, SIG_IGN);
    signal(SIGTTOU, SIG_IGN);
}

// Add new cleanup function
static void cleanup_background_processes() {
    // Set exiting flag to prevent further job status messages
    exiting = 1;
    
    // Terminate all background processes
    for (int i = 0; i < num_background_processes; i++) {
        pid_t pid = background_processes[i];
        kill(-pid, SIGTERM);
        waitpid(pid, NULL, 0);
    }
    num_background_processes = 0;
}

static void sigchld_handler(int __attribute__((unused)) sig) {
    pid_t pid;
    int status;
    char saved_input[SHELL_MAX_INPUT];
    size_t saved_cursor = (size_t)current_cursor_pos;  // Convert to size_t for comparison
    
    // Save current input state
    strncpy(saved_input, current_input_buffer, SHELL_MAX_INPUT);
    
    // Use WNOHANG to prevent blocking
    while ((pid = waitpid(-1, &status, WNOHANG | WUNTRACED)) > 0) {
        if (WIFSTOPPED(status)) {
            if (!is_background_process(pid)) {
                add_background_process(pid);
                printf("\r\033[K[%d] Stopped %s\n", get_job_number(pid), current_command);
            }
        } else if (WIFSIGNALED(status)) {
            printf("\r\033[KTerminated\n");
            if (is_background_process(pid)) {
                remove_background_process(pid);
            }
        } else if (WIFEXITED(status)) {
            if (is_background_process(pid)) {
                printf("\r\033[K[%d] Done %s\n", get_job_number(pid), get_process_command(pid));
                remove_background_process(pid);
            }
        }
    }

    // Only restore prompt and input if we were in input mode
    if (in_input && saved_input[0] != '\0') {
        printf("%s%s", current_prompt, saved_input);
        
        // Restore cursor position
        if (saved_cursor < strlen(saved_input)) {
            printf("\033[%luD", strlen(saved_input) - saved_cursor);
        }
        
        // Make sure the input buffer maintains its state
        strncpy(current_input_buffer, saved_input, SHELL_MAX_INPUT);
        current_input_length = strlen(saved_input);
        current_cursor_pos = saved_cursor;
        
        fflush(stdout);
    } else if (in_input) {
        print_prompt();
        fflush(stdout);
    }
}

static void sigint_handler(int __attribute__((unused)) sig) {
    if (foreground_pid > 0) {
        // Add to background list temporarily to track the command
        add_background_process(foreground_pid);
        
        // Send interrupt signal
        kill(-foreground_pid, SIGINT);
        
        // Print message
        printf("\r\033[KTerminated\n");
        
        // Remove from background list and clear foreground pid
        remove_background_process(foreground_pid);
        foreground_pid = 0;
    } else if (current_input_length > 0) {
        printf("\r\033[K^C\n");
    } else {
        printf("\r\033[K^C\n");
    }

    // Clear input state
    current_input_length = 0;
    current_cursor_pos = 0;
    current_input_buffer[0] = '\0';
    
    if (!foreground_pid) {
        print_prompt();
    }
    fflush(stdout);
}

// Add this function to set foreground pid
void set_foreground_pid(pid_t pid) {
    foreground_pid = pid;
}

static void sigtstp_handler(int __attribute__((unused)) sig) {
    if (foreground_pid > 0) {
        // If there's a foreground process, stop it
        kill(-foreground_pid, SIGTSTP);
        printf("\n[%d] Stopped %s\n", get_job_number(foreground_pid), current_command);
    } else {
        printf("\n");
    }
    fflush(stdout);
}

// Add signal handler for SIGTERM
static void sigterm_handler(int __attribute__((unused)) sig) {
    cleanup_background_processes();
    exit(EXIT_SUCCESS);
}

int main(int argc, char *argv[]) {
    // Set up SIGTERM handler
    signal(SIGTERM, sigterm_handler);
    
    shell_init();

    // Check if a script file was provided
    if (argc > 1) {
        if (is_script_file(argv[1])) {
            return execute_script(argv[1]);
        } else {
            fprintf(stderr, "Error: '%s' is not a .jsh script file\n", argv[1]);
            return 1;
        }
    }

    shell_loop();
    shell_cleanup();
    return 0;
}

// Update shell initialization to properly handle terminal control
void shell_init(void) {
    // Put shell in its own process group and grab control of the terminal
    pid_t shell_pgid = getpid();
    
    // Make sure we're running interactively
    if (isatty(STDIN_FILENO)) {
        // Loop until we're in the foreground
        while (tcgetpgrp(STDIN_FILENO) != (shell_pgid = getpgrp()))
            kill(-shell_pgid, SIGTTIN);

        // Ignore interactive and job-control signals
        signal(SIGINT, SIG_IGN);
        signal(SIGQUIT, SIG_IGN);
        signal(SIGTSTP, SIG_IGN);
        signal(SIGTTIN, SIG_IGN);
        signal(SIGTTOU, SIG_IGN);

        // Put ourselves in our own process group
        shell_pgid = getpid();
        if (setpgid(shell_pgid, shell_pgid) < 0) {
            perror("Couldn't put the shell in its own process group");
            exit(1);
        }

        // Grab control of the terminal
        tcsetpgrp(STDIN_FILENO, shell_pgid);
        
        // Set up all signal handlers
        set_signal_handlers();
    }

    struct sigaction sa;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART | SA_NOCLDSTOP;  // Don't create zombies for stopped children
    sa.sa_handler = sigchld_handler;
    sigaction(SIGCHLD, &sa, NULL);

    // Set shell name before loading RC file
    setenv("SHELL_NAME", "jshell", 1);
    
    history_init();
    history_load();  // Load history at startup
    alias_init();
    load_rc_file();

    // Ensure PATH includes system directories
    char *current_path = getenv("PATH");
    if (current_path) {
        char new_path[4096];
        snprintf(new_path, sizeof(new_path), 
                "/usr/local/bin:/usr/bin:/bin:/usr/sbin:/sbin:%s", 
                current_path);
        setenv("PATH", new_path, 1);
    } else {
        setenv("PATH", "/usr/local/bin:/usr/bin:/bin:/usr/sbin:/sbin", 1);
    }

    // Add this before the first prompt
    printf("Welcome to JShell! Type 'help' for available commands.\n");
}

// Update shell_loop to handle exit properly
void shell_loop(void) {
    char *input;
    command_t *cmd;
    int status = 1;

    while (status && !exiting) {
        print_prompt();
        current_input_length = 0;
        current_input_buffer[0] = '\0';

        in_input = 1;  // Mark we're waiting for input.
        input = read_input();
        in_input = 0;  // Clear flag after input is received.

        if (!input) {
            printf("\n");
            break;
        }
        if (strlen(input) == 0) {
            free(input);
            continue;
        }
        cmd = parse_input(input);
        if (cmd && cmd->command) {
            if (strcmp(cmd->command, "exit") == 0) {
                cleanup_background_processes();
                status = 0;
            } else {
                strncpy(current_command, cmd->args[0], MAX_CMD_LEN - 1);
                execute_command(cmd);
            }
        }
        free(input);
        command_free(cmd);
    }
}

void shell_cleanup(void) {
    alias_cleanup();
    cleanup_background_processes();
    history_cleanup();
}
