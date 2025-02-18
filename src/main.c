#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>
#include <signal.h>
#include <termios.h>
#include <errno.h>

// Project headers
#include "shell.h"
#include "history.h"
#include "builtin_commands.h"
#include "command_registry.h"
#include "job_manager.h"

// Add signal handler declarations at the top before any functions
static void sigchld_handler(int);
static void sigint_handler(int);
static void sigterm_handler(int);
static void sigtstp_handler(int);

// Change these from static to global
int current_cursor_pos = 0;
char current_prompt[SHELL_MAX_INPUT];

// Remove legacy declarations:

// Define the global current_command variable (matches the extern declaration)
char current_command[MAX_CMD_LEN];

// Define these as global variables for input.c to use
char current_input_buffer[SHELL_MAX_INPUT];
int current_input_length = 0;

// Add a global flag for exit state
static volatile int exiting = 0;

volatile int in_input = 0;  
pid_t foreground_pid = 0; 

// Add global foreground wait flag:
volatile int fg_wait = 0;

void print_prompt(void) {
    char cwd[PATH_MAX];
    char prompt_buf[MAX_PROMPT_LEN];
    
    if (getcwd(cwd, sizeof(cwd)) == NULL) {
        strcpy(cwd, "???");
    }
    
    size_t prompt_result = snprintf(prompt_buf, sizeof(prompt_buf), "%s %s", cwd, SHELL_PROMPT);
    if (prompt_result >= sizeof(prompt_buf)) {
        strcpy(current_prompt, SHELL_PROMPT);
        printf("%s ", SHELL_PROMPT);
        return;
    }
    
    strcpy(current_prompt, prompt_buf);
    printf("%s ", prompt_buf);
}

static void set_signal_handlers() {
    struct sigaction sa;
    sigemptyset(&sa.sa_mask);
    
    // Disable SA_RESTART for SIGCHLD so that blocking input is interrupted
    sa.sa_flags = SA_RESTART | SA_NOCLDSTOP;  // Don't create zombies for stopped children
    sa.sa_handler = sigchld_handler;
    sigaction(SIGCHLD, &sa, NULL);
    
    sa.sa_handler = sigint_handler;
    sigaction(SIGINT, &sa, NULL);

    sa.sa_handler = sigtstp_handler;
    sigaction(SIGTSTP, &sa, NULL);

    signal(SIGQUIT, SIG_IGN);  // Ignore quit signal
    signal(SIGTTIN, SIG_IGN);  // Ignore background read
    signal(SIGTTOU, SIG_IGN);  // Ignore background write
}

void cleanup_background_processes(void) {
    exiting = 1;
    int count = 0;
    job_t *job_list = job_manager_get_all_jobs(&count);
    for (int i = 0; i < count; i++) {
        pid_t pid = job_list[i].pid;
        kill(-pid, SIGTERM);
        waitpid(pid, NULL, 0);
    }
}

int get_job_number(pid_t pid) {
    job_t *job = job_manager_get_job_by_pid(pid);
    return job ? job->job_id : -1;
}

int get_pid_by_job_id(int job_id) {
    int count = 0;
    job_t *job_list = job_manager_get_all_jobs(&count);
    for (int i = 0; i < count; i++) {
        if (job_list[i].job_id == job_id)
            return job_list[i].pid;
    }
    return -1;
}

char *get_process_command(pid_t pid) {
    job_t *job = job_manager_get_job_by_pid(pid);
    return job ? job->command : "unknown";
}

static void sigchld_handler(int __attribute__((unused)) sig) {
    pid_t pid;
    int status;
    int bg_finished = 0;  // Flag for background job completion
    char saved_input[SHELL_MAX_INPUT];
    size_t saved_cursor = (size_t)current_cursor_pos;

    // Save current input state
    strncpy(saved_input, current_input_buffer, SHELL_MAX_INPUT);

    while ((pid = waitpid(-1, &status, WNOHANG | WUNTRACED | WCONTINUED)) > 0) {
        job_t *job = job_manager_get_job_by_pid(pid);
        if (job) {
            if (WIFSTOPPED(status)) {
                job_manager_update_state(pid, JOB_STOPPED);
                printf("\r\033[K[%d] Suspended %s\n", job->job_id, job->command);
            } else if (WIFSIGNALED(status)) {
                bg_finished = bg_finished || job->background;
                printf("\r\033[K[%d] Terminated %s\n", job->job_id, job->command);
                job_manager_remove_job(pid);
            } else if (WIFEXITED(status)) {
                bg_finished = bg_finished || job->background;
                printf("\r\033[K[%d] Done %s\n", job->job_id, job->command);
                job_manager_remove_job(pid);
            }
        }
    }
    
    // Only print a prompt if not waiting for a foreground job.
    if (!fg_wait) {
        if (in_input && bg_finished) {
            print_prompt();
            printf("%s", saved_input);
            if (saved_cursor < strlen(saved_input)) {
                printf("\033[%luD", strlen(saved_input) - saved_cursor);
            }
            fflush(stdout);
        } else if (bg_finished && !in_input) {
            print_prompt();
            fflush(stdout);
        } else if (in_input && saved_input[0] != '\0') {
            printf("%s%s", current_prompt, saved_input);
            if (saved_cursor < strlen(saved_input)) {
                printf("\033[%luD", strlen(saved_input) - saved_cursor);
            }
            fflush(stdout);
        }
    }
}


static void sigint_handler(int __attribute__((unused)) sig) {
    if (foreground_pid > 0) {
        kill(-foreground_pid, SIGINT);
        printf("\r\033[KTerminated\n");
        foreground_pid = 0;
    } else {
        printf("\r\033[K^C\n");
    }
    current_input_length = 0;
    current_cursor_pos = 0;
    current_input_buffer[0] = '\0';
    if (!foreground_pid) {
        print_prompt();
    }
    fflush(stdout);
}

static void sigtstp_handler(int __attribute__((unused)) sig) {
    if (foreground_pid > 0) {
        kill(-foreground_pid, SIGTSTP);
        printf("\r\033[K[%d] Suspended\n", foreground_pid);
        foreground_pid = 0;
    } else {
        printf("\r\033[K^Z\n");
    }

    // Clear input state
    current_input_length = 0;
    current_cursor_pos = 0;
    current_input_buffer[0] = '\0';

    print_prompt();
    fflush(stdout);
}

// Add this function to set foreground pid
void set_foreground_pid(pid_t pid) {
    foreground_pid = pid;
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
    // Put shell in its own process group and grab terminal control
    pid_t shell_pgid = getpid();
    if (isatty(STDIN_FILENO)) {
        while (tcgetpgrp(STDIN_FILENO) != (shell_pgid = getpgrp()))
            kill(-shell_pgid, SIGTTIN);
        signal(SIGINT, SIG_IGN);
        signal(SIGQUIT, SIG_IGN);
        signal(SIGTSTP, sigtstp_handler);
        signal(SIGTTIN, SIG_IGN);
        signal(SIGTTOU, SIG_IGN);
        shell_pgid = getpid();
        if (setpgid(shell_pgid, shell_pgid) < 0) {
            perror("Couldn't put the shell in its own process group");
            exit(1);
        }
        tcsetpgrp(STDIN_FILENO, shell_pgid);
        set_signal_handlers();
    }
    {
        struct sigaction sa;
        sigemptyset(&sa.sa_mask);
        sa.sa_flags = SA_RESTART | SA_NOCLDSTOP;
        sa.sa_handler = sigchld_handler;
        sigaction(SIGCHLD, &sa, NULL);
    }
    setenv("SHELL_NAME", "jshell", 1);
    history_init();
    history_load();
    alias_init();
    init_command_registry();
    register_builtin_commands();
    load_rc_file();
    if (!getenv("PATH")) {
        setenv("PATH", "/usr/local/bin:/usr/bin:/bin:/usr/sbin:/sbin", 1);
    }
    printf("Welcome to JShell! Type 'help' for available commands.\n");
}

void shell_cleanup(void) {
    // Clean up all subsystems
    history_save();
    history_cleanup();
    alias_cleanup();
    cleanup_command_registry();
    cleanup_background_processes();
    
    // Reset terminal control
    tcsetpgrp(STDIN_FILENO, getpgrp());
    
    printf("\nGoodbye!\n");
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
        if (cmd) {
            // Now, if the command is an if block, execute it regardless of cmd->command.
            if (cmd->type == CMD_IF || cmd->type == CMD_WHILE || cmd->type == CMD_FOR) {
                execute_command(cmd);
            }
            else if (cmd->command) {
                if (strcmp(cmd->command, "exit") == 0) {
                    cleanup_background_processes();
                    status = 0;
                } else {
                    strncpy(current_command, cmd->args[0], MAX_CMD_LEN - 1);
                    execute_command(cmd);
                }
            }
        }
        free(input);
        command_free(cmd);
    }
}
