/*
 * Copyright 2016 Gauthier Voron <gauthier.voron@lip6.fr>
 * This file is part of pin.
 *
 * Pin is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Pin is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with pin.  If not, see <http://www.gnu.org/licenses/>.
 */

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
