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

// Signal handlers
static void sigchld_handler(int);
static void sigint_handler(int);
static void sigterm_handler(int);
static void sigtstp_handler(int);

int current_cursor_pos = 0;
char current_prompt[SHELL_MAX_INPUT];
char current_command[MAX_CMD_LEN];
char current_input_buffer[SHELL_MAX_INPUT];
int current_input_length = 0;
static volatile int exiting = 0;
volatile int in_input = 0;  
pid_t foreground_pid = 0;
volatile int fg_wait = 0, print_prompt_pending = 0, fg_process_done = 0;
int command_mode = 0;

void print_prompt(void) {
    char cwd[PATH_MAX];
    char prompt_buf[MAX_PROMPT_LEN];
    // Check if getcwd fails and report the error.
    if (getcwd(cwd, sizeof(cwd)) == NULL) {
        perror("getcwd failed");
        // Fallback: use HOME or a known directory.
        char *home = getenv("HOME");
        if (home)
            strncpy(cwd, home, sizeof(cwd));
        else
            strcpy(cwd, "~");
    }
    snprintf(prompt_buf, sizeof(prompt_buf), "%s %s", cwd, SHELL_PROMPT);
    strcpy(current_prompt, prompt_buf);
    printf("%s", prompt_buf);
}

static void set_signal_handlers() {
    struct sigaction sa;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART | SA_NOCLDSTOP;
    sa.sa_handler = sigchld_handler;
    sigaction(SIGCHLD, &sa, NULL);
    
    sa.sa_handler = sigint_handler;
    sigaction(SIGINT, &sa, NULL);
    
    sa.sa_handler = sigtstp_handler;
    sigaction(SIGTSTP, &sa, NULL);
    
    signal(SIGQUIT, SIG_IGN);
    signal(SIGTTIN, SIG_IGN);
    signal(SIGTTOU, SIG_IGN);
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
    char saved_input[SHELL_MAX_INPUT];
    size_t saved_cursor = (size_t)current_cursor_pos;
    strncpy(saved_input, current_input_buffer, SHELL_MAX_INPUT);
    while ((pid = waitpid(-1, &status, WNOHANG | WUNTRACED | WCONTINUED)) > 0) {
        job_t *job = job_manager_get_job_by_pid(pid);
        if (job) {
            if (WIFSTOPPED(status)) {
                job_manager_update_state(pid, JOB_STOPPED);
                printf("\r\033[K[%d] Suspended %s\n", job->job_id, job->command);
                print_prompt_pending = 1;
            } else if (WIFSIGNALED(status) || WIFEXITED(status)) {
                if (pid == foreground_pid) fg_process_done = 1;
                else {
                    printf("\r\033[K[%d] Done %s\n", job->job_id, job->command);
                    print_prompt_pending = 1;
                }
                job_manager_remove_job(pid);
            }
        }
    }
    if (print_prompt_pending && !fg_wait) {
        if (in_input) {
            print_prompt();
            printf("%s", saved_input);
            if (saved_cursor < strlen(saved_input))
                printf("\033[%luD", strlen(saved_input) - saved_cursor);
        } else {
            print_prompt();
        }
        fflush(stdout);
        print_prompt_pending = 0;
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
    if (!foreground_pid) print_prompt();
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
    current_input_length = 0;
    current_cursor_pos = 0;
    current_input_buffer[0] = '\0';
    print_prompt();
    fflush(stdout);
}

void set_foreground_pid(pid_t pid) { foreground_pid = pid; }

static void sigterm_handler(int __attribute__((unused)) sig) {
    cleanup_background_processes();
    exit(EXIT_SUCCESS);
}

int main(int argc, char *argv[]) {
    signal(SIGTERM, sigterm_handler);
    if (argc > 1 && strcmp(argv[1], "-c") == 0) command_mode = 1;
    shell_init();
    if (argc > 1) {
        if (strcmp(argv[1], "-c") == 0) {
            if (argc < 3) {
                fprintf(stderr, "Error: Missing command after -c\n");
                shell_cleanup();
                exit(EXIT_FAILURE);
            }
            command_t *cmd = parse_input(argv[2]);
            if (cmd) {
                execute_command(cmd);
                int exit_status = cmd->last_status;
                command_free(cmd);
                shell_cleanup();
                exit(exit_status);
            } else {
                fprintf(stderr, "Error: Failed to parse command\n");
                shell_cleanup();
                exit(EXIT_FAILURE);
            }
        } else if (is_script_file(argv[1])) {
            return execute_script(argv[1]);
        } else {
            fprintf(stderr, "Error: Unrecognized option or file '%s'\n", argv[1]);
            shell_cleanup();
            exit(EXIT_FAILURE);
        }
    }
    shell_loop();
    shell_cleanup();
    return 0;
}

void shell_init(void) {
    // ...existing code...
    if (isatty(STDIN_FILENO)) {
        pid_t shell_pgid = getpid();
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
    // ...existing code...
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
    if (!command_mode) printf("Welcome to JShell! Type 'help' for available commands.\n");
}

void shell_cleanup(void) {
    history_save();
    history_cleanup();
    alias_cleanup();
    cleanup_command_registry();
    cleanup_background_processes();
    tcsetpgrp(STDIN_FILENO, getpgrp());
    if (!command_mode) printf("\nGoodbye!\n");
}

void shell_loop(void) {
    char *input;
    command_t *cmd;
    int status = 1;
    while (status && !exiting) {
        print_prompt();
        current_input_length = 0;
        current_input_buffer[0] = '\0';
        in_input = 1;
        input = read_input();
        in_input = 0;
        if (!input) { printf("\n"); break; }
        if (strlen(input) == 0) { free(input); continue; }
        cmd = parse_input(input);
        if (cmd) {
            if (cmd->type == CMD_IF || cmd->type == CMD_WHILE || cmd->type == CMD_FOR ||
                cmd->type == CMD_CASE || cmd->type == CMD_SUBSHELL ||
                cmd->type == CMD_AND || cmd->type == CMD_OR || cmd->type == CMD_SEQUENCE) {
                execute_command(cmd);
            } else if (cmd->command) {
                if (strcmp(cmd->command, "exit") == 0) { cleanup_background_processes(); status = 0; }
                else { strncpy(current_command, cmd->args[0], MAX_CMD_LEN - 1); execute_command(cmd); }
            }
        }
        free(input);
        command_free(cmd);
    }
}
