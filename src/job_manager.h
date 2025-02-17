#ifndef JOB_MANAGER_H
#define JOB_MANAGER_H

#include <sys/types.h>
#include "shell.h"  // for MAX_CMD_LEN

typedef enum {
    JOB_RUNNING,
    JOB_STOPPED,
    JOB_DONE
} job_state_t;

typedef struct job {
    int job_id;
    pid_t pid;
    char command[MAX_CMD_LEN];
    job_state_t state;
    int background; // 1 if background job, 0 if foreground suspended
} job_t;

// Updated: Add background flag parameter.
void job_manager_add_job(pid_t pid, const char *command, int background);
job_t *job_manager_get_job_by_pid(pid_t pid);
void job_manager_update_state(pid_t pid, job_state_t state);
void job_manager_remove_job(pid_t pid);
// New function to obtain the job list
job_t *job_manager_get_all_jobs(int *count);

#endif
