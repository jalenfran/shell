#ifndef JOB_OBSERVER_H
#define JOB_OBSERVER_H

#include <sys/types.h>

// Job observer functions
void job_observer_notify_started(pid_t pid, const char* cmd);
void job_observer_notify_stopped(pid_t pid, const char* cmd);
void job_observer_notify_completed(pid_t pid, const char* cmd, int status);
void job_observer_init(void);
void job_observer_cleanup(void);

// Observer structure
typedef struct {
    void (*on_job_started)(pid_t pid, const char* cmd);
    void (*on_job_stopped)(pid_t pid, const char* cmd);
    void (*on_job_completed)(pid_t pid, const char* cmd, int status);
} job_observer_t;

void job_observer_register(job_observer_t* observer);

#endif
