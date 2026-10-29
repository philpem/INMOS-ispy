#include "job.h"

void job_init(struct job *job)
{
    job->busy = job->cancel = job->quitting = job->live = 0;
}

int job_begin(struct job *job)
{
    if (job->busy || job->quitting) return 0;
    job->busy = 1;
    job->cancel = 0;
    job->live = 0;
    return 1;
}

void job_cancel(struct job *job)
{
    if (job->busy) job->cancel = 1;
}

void job_quit(struct job *job)
{
    job->quitting = 1;
    job_cancel(job);
}

void job_end(struct job *job, int prepared)
{
    job->live = job->busy && prepared && !job->cancel && !job->quitting;
    job->busy = 0;
}

int job_can_quit(const struct job *job)
{
    return job->quitting && !job->busy;
}
