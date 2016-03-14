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

#define _GNU_SOURCE

#include <pin.h>

#include <dlfcn.h>
#include <pthread.h>
#include <sched.h>
#include <stdlib.h>


static int (*__pthread_create)(pthread_t *thread, const pthread_attr_t *attr,
			       void *(*start_routine)(void *), void *arg);

static int (*__sched_setaffinity)(pid_t pid, size_t cpusetsize,
				  const cpu_set_t *mask);

static int (*__sched_getaffinity)(pid_t pid, size_t cpusetsize,
				  cpu_set_t *mask);

static int (*__sched_getcpu)(void);


static inline void load_functions(void)
{
	__pthread_create = dlsym(RTLD_NEXT, "pthread_create");
	__sched_setaffinity = dlsym(RTLD_NEXT, "sched_setaffinity");
	__sched_getaffinity = dlsym(RTLD_NEXT, "sched_getaffinity");
	__sched_getcpu = dlsym(RTLD_NEXT, "sched_getcpu");
}


static inline int original_create(pthread_t *thread,
				  const pthread_attr_t *attr,
				  void *(*start_routine) (void *), void *arg)
{
	return __pthread_create(thread, attr, start_routine, arg);
}

static inline int original_setaffinity(pid_t pid, size_t cpusetsize,
				       const cpu_set_t *mask)
{
	return __sched_setaffinity(pid, cpusetsize, mask);
}

static inline int original_getaffinity(pid_t pid, size_t cpusetsize,
				       cpu_set_t *mask)
{
	return __sched_getaffinity(pid, cpusetsize, mask);
}

static inline int original_getcpu(void)
{
	return __sched_getcpu();
}


int pthread_create(pthread_t *thread, const pthread_attr_t *attr,
		   void *(*start_routine) (void *), void *arg)
{
	const cpu_set_t *set;
	int ret;

	ret = original_create(thread, attr, start_routine, arg);

	if ((set = get_next_cpumask()) != NULL)
		pthread_setaffinity_np(*thread, sizeof (*set), set);

	return ret;
}

int sched_setaffinity(pid_t pid, size_t cpusetsize, const cpu_set_t *mask)
{
	cpu_set_t *nmask = alloca(cpusetsize);

	map_cpuset_forward(nmask, mask, cpusetsize);
	return original_setaffinity(pid, cpusetsize, nmask);
}

int sched_getaffinity(pid_t pid, size_t cpusetsize, cpu_set_t *mask)
{
	cpu_set_t *nmask = alloca(cpusetsize);
	int ret = original_getaffinity(pid, cpusetsize, nmask);

	if (ret == 0)
		map_cpuset_reverse(mask, nmask, cpusetsize);
	return ret;
}

int sched_getcpu(void)
{
	int cpu = original_getcpu();
	return map_cpu_reverse(cpu);
}


static void __attribute__((constructor)) init(void)
{
	const cpu_set_t *set;
	
	acquire_arguments();
	load_functions();

	if ((set = get_next_cpumask()) != NULL)
		pthread_setaffinity_np(pthread_self(), sizeof (*set), set);
}
