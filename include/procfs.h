#ifndef PIN_PROCFS_H
#define PIN_PROCFS_H


#include <unistd.h>


#define PID_MAXLEN  5
#define TID_MAXLEN  5


typedef pid_t tid_t;


struct task_stat
{
	pid_t          pid;
	const char    *name;
	char           state;
	pid_t          ppid;
	unsigned int   core;
};

int for_tid_stat(pid_t pid, tid_t tid,
		 int (*cb)(pid_t, tid_t, const struct task_stat *, void *),
		 void *data);

int for_pid_stat(pid_t pid,
		 int (*cb)(pid_t, const struct task_stat *, void *),
		 void *data);


int foreach_pid(int (*cb)(pid_t, void *), void *data);

int foreach_tid(pid_t pid, int (*cb)(pid_t, tid_t, void *), void *data);


#endif
