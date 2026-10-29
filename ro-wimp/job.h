#ifndef ISPY_JOB_H
#define ISPY_JOB_H

/* One owner, including while nested WIMP event dispatch is on the stack. */
struct job {
    int busy;
    int cancel;
    int quitting;
    int live;
};

void job_init(struct job *job);
int job_begin(struct job *job);
void job_cancel(struct job *job);
void job_quit(struct job *job);
void job_end(struct job *job, int prepared);
int job_can_quit(const struct job *job);

#endif
