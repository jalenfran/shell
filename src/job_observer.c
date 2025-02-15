#include "job_observer.h"
#include <stdio.h>
#include <stdlib.h>

#define MAX_OBSERVERS 10

static job_observer_t* observers[MAX_OBSERVERS];
static int observer_count = 0;

void job_observer_init(void) {
    observer_count = 0;
}

void job_observer_register(job_observer_t* observer) {
    if (observer_count < MAX_OBSERVERS) {
        observers[observer_count++] = observer;
    }
}

void job_observer_notify_started(pid_t pid, const char* cmd) {
    for (int i = 0; i < observer_count; i++) {
        if (observers[i]->on_job_started) {
            observers[i]->on_job_started(pid, cmd);
        }
    }
}

void job_observer_notify_stopped(pid_t pid, const char* cmd) {
    for (int i = 0; i < observer_count; i++) {
        if (observers[i]->on_job_stopped) {
            observers[i]->on_job_stopped(pid, cmd);
        }
    }
}

void job_observer_notify_completed(pid_t pid, const char* cmd, int status) {
    for (int i = 0; i < observer_count; i++) {
        if (observers[i]->on_job_completed) {
            observers[i]->on_job_completed(pid, cmd, status);
        }
    }
}

void job_observer_cleanup(void) {
    observer_count = 0;
}
