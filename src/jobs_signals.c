#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>
#include <string.h>
#include "job_manager.h"
#include "shell.h"

extern pid_t foreground_pid;  // defined in main.c
extern volatile int in_input; // global flag indicating input mode
extern char current_input_buffer[SHELL_MAX_INPUT]; // current input buffer
extern char current_prompt[SHELL_MAX_INPUT]; // current prompt text

static struct sigaction old_sigint;
static struct sigaction old_sigtstp;
static struct sigaction old_sigchld;

// Simple safe print function.
static void safe_print(const char *msg) {
    if (in_input) {
        // Move cursor to new line, clear, print msg then reprint prompt and input
        printf("\r\033[K%s\n%s%s", msg, current_prompt, current_input_buffer);
        fflush(stdout);
    } else {
        printf("\n%s\n", msg);
        fflush(stdout);
    }
}

static void sigchld_handler(int sig) {
    (void)sig;
    int status;
    pid_t pid;
    while ((pid = waitpid(-1, &status, WNOHANG | WUNTRACED | WCONTINUED)) > 0) {
        job_t *job = job_manager_get_job_by_pid(pid);
        if (!job)
            continue;
        if (WIFSTOPPED(status)) {
            job_manager_update_state(pid, JOB_STOPPED);
            {
                char buf[256];
                snprintf(buf, sizeof(buf), "[%d] Suspended %s", job->job_id, job->command);
                safe_print(buf);
            }
        } else if (WIFSIGNALED(status) || WIFEXITED(status)) {
            job_manager_update_state(pid, JOB_DONE);
            {
                char buf[256];
                snprintf(buf, sizeof(buf), "[%d] Done %s", job->job_id, job->command);
                safe_print(buf);
            }
            job_manager_remove_job(pid);
        } else if (WIFCONTINUED(status)) {
            job_manager_update_state(pid, JOB_RUNNING);
            {
                char buf[256];
                snprintf(buf, sizeof(buf), "[%d] Running %s", job->job_id, job->command);
                safe_print(buf);
            }
        }
    }
}

static void sigint_handler(int sig) {
    (void)sig;
    if (foreground_pid > 0) {
        kill(-foreground_pid, SIGINT);
    }
}

static void sigtstp_handler(int sig) {
    (void)sig;
    if (foreground_pid > 0) {
        kill(-foreground_pid, SIGTSTP);
    }
}

void init_jobs_signals(void) {
    struct sigaction sa;
    
    /* Setup SIGCHLD handler */
    sa.sa_handler = sigchld_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART | SA_NOCLDSTOP;
    if (sigaction(SIGCHLD, &sa, &old_sigchld) == -1) {
        perror("sigaction SIGCHLD");
        exit(1);
    }

    /* Setup SIGINT handler */
    sa.sa_handler = sigint_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    if (sigaction(SIGINT, &sa, &old_sigint) == -1) {
        perror("sigaction SIGINT");
        exit(1);
    }

    /* Setup SIGTSTP handler */
    sa.sa_handler = sigtstp_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    if (sigaction(SIGTSTP, &sa, &old_sigtstp) == -1) {
        perror("sigaction SIGTSTP");
        exit(1);
    }
}

void cleanup_jobs_signals(void) {
    sigaction(SIGCHLD, &old_sigchld, NULL);
    sigaction(SIGINT, &old_sigint, NULL);
    sigaction(SIGTSTP, &old_sigtstp, NULL);
}
