#ifndef JOBS_H
#define JOBS_H

#include <sys/types.h>
#include <stddef.h>

#define MAX_JOBS 64

typedef enum {
    JOB_RUNNING,
    JOB_STOPPED,
    JOB_DONE
} job_state_t;

typedef struct {
    int id;
    pid_t pgid;
    job_state_t state;
    char *command;
} job_t;

int jobs_init(void);
void jobs_shutdown(void);
void jobs_sigchld_handler(int signo);
void jobs_reap(void);
int jobs_add(pid_t pgid, job_state_t state, const char *command);
int jobs_update_pgid(pid_t pgid, job_state_t state);
int jobs_remove(int id);
void jobs_print(void);
job_t *jobs_get(int id);
job_t *jobs_current(void);
job_t *jobs_previous(void);
int jobs_fg(int id);
int jobs_bg(int id);
int jobs_mark_done(pid_t pgid);
int jobs_has_active(void);

pid_t shell_pgid(void);
int shell_terminal(void);
int shell_interactive(void);

#endif
