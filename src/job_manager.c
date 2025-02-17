#include "job_manager.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#define MAX_JOBS 128

static job_t jobs[MAX_JOBS];
static int num_jobs = 0;
static int next_job_id = 1;

// Updated job_manager_add_job with background flag:
void job_manager_add_job(pid_t pid, const char *command, int background) {
    if (num_jobs < MAX_JOBS) {
        job_t new_job;
        new_job.job_id = next_job_id++;
        new_job.pid = pid;
        strncpy(new_job.command, command, MAX_CMD_LEN - 1);
        new_job.command[MAX_CMD_LEN - 1] = '\0';
        new_job.state = JOB_RUNNING;
        new_job.background = background;
        jobs[num_jobs++] = new_job;
    }
}

job_t *job_manager_get_job_by_pid(pid_t pid) {
    for (int i = 0; i < num_jobs; i++) {
        if (jobs[i].pid == pid)
            return &jobs[i];
    }
    return NULL;
}

void job_manager_update_state(pid_t pid, job_state_t state) {
    job_t *job = job_manager_get_job_by_pid(pid);
    if (job) {
        job->state = state;
    }
}

void job_manager_remove_job(pid_t pid) {
    int index = -1;
    for (int i = 0; i < num_jobs; i++) {
        if (jobs[i].pid == pid) {
            index = i;
            break;
        }
    }
    if (index != -1) {
        for (int i = index; i < num_jobs - 1; i++) {
            jobs[i] = jobs[i + 1];
        }
        num_jobs--;
    }
}

job_t *job_manager_get_all_jobs(int *count) {
    if (count) *count = num_jobs;
    return jobs;
}
