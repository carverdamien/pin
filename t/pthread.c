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

#include <pthread.h>
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


#define SECOND       (1000000000ul)
#define WORK_TIME    (10 * SECOND)

static size_t      thread_count = 0;
static cpu_set_t  *thread_pinnings = NULL;


static void usage(void)
{
	printf("Usage: pthread [<thread-spec...>]\n"
	       "Launch one or more threads making useless work for 10 "
	       "seconds. When done, each\n"
	       "thread prints the mask of the cores they ran on (in "
	       "hexadecimal).\n\n");
	printf("If no thread specification is supplied, only the main thread "
	       "is used with a\n"
	       "default specifiaction, otherwise the first specification is "
	       "used for the main\n"
	       "thread and additional threads are launched with the "
	       "following specifications.\n\n");
	printf("A thread specification is a mask indicating on what core to "
	       "pin the thread.\n");
}


static unsigned long gettime()
{
	struct timespec ts;

	clock_gettime(CLOCK_REALTIME, &ts);
	return ts.tv_sec * SECOND + ts.tv_nsec;
}

static void *work(void *arg)
{
	cpu_set_t *mask = (cpu_set_t *) arg;
	unsigned long start = gettime();
	int core;

	sched_setaffinity(0, sizeof (*mask), mask);
	CPU_ZERO(mask);

	while (gettime() < start + WORK_TIME) {
		core = sched_getcpu();
		CPU_SET(core, mask);
	}

	return NULL;
}

static void parse_arguments(int argc, const char **argv)
{
	cpu_set_t zero;
	size_t j, hexa;
	const char *name = argv[0];
	char *err;
	int i;

	if (argc == 1) {
		thread_count = 1;
		thread_pinnings = malloc(sizeof (*thread_pinnings));
		if (thread_pinnings == NULL)
			abort();

		CPU_ZERO(&zero);
		CPU_ZERO(thread_pinnings);
		CPU_XOR(thread_pinnings, thread_pinnings, &zero);

		return;
	}

	if (!strcmp(argv[1], "--help") || !strcmp(argv[1], "-h")) {
		usage();
		exit(EXIT_SUCCESS);
	}

	thread_count = argc - 1;
	thread_pinnings = malloc(sizeof (*thread_pinnings) * thread_count);
	if (thread_pinnings == NULL)
		abort();

	argv++;
	argc--;
	for (i=0; i<argc; i++) {
		hexa = strtol(argv[i], &err, 16);
		if (*err != '\0') {
			fprintf(stderr, "%s: invalid spec '%s'\n"
				"Please type '%s --help' for more "
				"informations\n", name, argv[i], name);
			exit(EXIT_FAILURE);
		}

		CPU_ZERO(thread_pinnings + i);
		for (j=0; j<(sizeof (hexa) << 3); j++) {
			if (hexa & (1ul << j))
				CPU_SET(j, thread_pinnings + i);
		}
	}
}

static void display_mask(const cpu_set_t *mask)
{
	char cores;
	size_t i, j;
	int display = 0;

	for (i = sizeof (*mask) - 1; i < sizeof (*mask); i--) {
		cores = 0;
		for (j=4; j<8; j++)
			if (CPU_ISSET(8 * i + j, mask))
				cores |= (1 << (j - 4));
		
		if (cores != 0)
			display = 1;
		if (display)
			printf("%x", cores);

		cores = 0;
		for (j=0; j<4; j++)
			if (CPU_ISSET(8 * i + j, mask))
				cores |= (1 << j);
		
		if (cores != 0)
			display = 1;
		if (display)
			printf("%x", cores);
	}

	printf("\n");
}

int main(int argc, const char **argv)
{
	size_t i;
	pthread_t *tids;

	parse_arguments(argc, argv);
	tids = malloc(sizeof (pthread_t) * (thread_count - 1));

	for (i=0; i < (thread_count - 1); i++)
		pthread_create(&tids[i], NULL, work, thread_pinnings + i + 1);

	work(thread_pinnings);

	for (i=0; i < (thread_count - 1); i++)
		pthread_join(tids[i], NULL);

	for (i=0; i<thread_count; i++)
		display_mask(thread_pinnings + i);
	
	return EXIT_SUCCESS;
}
