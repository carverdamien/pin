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

#include <dirent.h>
#include <getopt.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#include "procfs.h"


#define PROGNAME "scanpin"

#define SLURP_CHUNK 256
#define PIDS_CHUNK  16


const char *progname;

size_t  children = 0;
size_t  default_children = 10;

char    print_name = 0;

pid_t  *pids_to_scan = NULL;
size_t  pids_capacity = 0;
size_t  pids_length = 0;

size_t  scan_every_ms = 100;
size_t  current_time;


static void usage(void)
{
	printf("Usage: %s [options] <pid>\n"
	       "Scan periodically the cores used the process "
	       "with the specified pid.\n"
	       "The output is printed on the standard output and has the "
	       "form:\n\n"
	       "  <time>:<pid>:<tid>:<core>\n\n"
	       "At each collecting, the <time> in millisecond from the start "
	       "is printed, then\n"
	       "for each thread of the process, the <tid> is specified aside "
	       "of the used <core>.\n", progname);
	printf("The scan will stop automatically when there is no more "
	       "process to track or until\n"
	       "it receive a SIGINT.\n\n");
	printf("Options:\n"
	       "  -h, --help             Print this help message and exit\n"
	       "  -V, --version          Print the version message and exit\n"
	       "  -p, --period=<ms>      Collect information every <ms> "
	       "millisecond\n"
	       "                         [default = %lu]\n"
	       "  -c, --children[=<n>]   Collect information for children "
	       "too. Only scan for\n"
	       "                         children once every <n> period "
	       "[default = %lu]\n"
	       "  -n, --name             Print the name of the tracked processes with lines:\n"
	       "                         <time>:<pid>=<name>\n",
	       scan_every_ms, default_children);
}

static void version(void)
{
	printf("%s %s\n%s\n%s\n", PROGNAME, VERSION, AUTHOR, EMAIL);
}


static void error(const char *format, ...)
{
	va_list ap;

	fprintf(stderr, "%s: ", progname);

	va_start(ap, format);
	vfprintf(stderr, format, ap);
	va_end(ap);

	fprintf(stderr, "\nPlease type '%s --help' for more informations\n",
		progname);

	exit(EXIT_FAILURE);
}

static void warning(const char *format, ...)
{
	va_list ap;

	fprintf(stderr, "%s: ", progname);

	va_start(ap, format);
	vfprintf(stderr, format, ap);
	va_end(ap);

	fprintf(stderr, "\n");
}


static void clean_exit(void)
{
	fflush(stdout);
	exit(EXIT_SUCCESS);
}

static void signal_exit(int signum __attribute__((unused)))
{
	clean_exit();
}


static int track_pid(pid_t pid)
{
	if (pids_length == pids_capacity) {
		pids_capacity += PIDS_CHUNK;
		pids_to_scan = realloc(pids_to_scan, sizeof (pid_t)
				       * pids_capacity);
		if (pids_to_scan == NULL)
			error("memory allocation failed for %lu",
			      sizeof (pid_t) * pids_capacity);
	}

	pids_to_scan[pids_length++] = pid;
	return 0;
}

static void untrack_pid(size_t index)
{
	pids_to_scan[index] = pids_to_scan[--pids_length];
}



static int print_core_stat_handler(pid_t pid, tid_t tid,
				   const struct task_stat *stat,
				   void *data)
{
	printf("%lu:%d:%d:%u\n", *((size_t *) data), pid, tid, stat->core);
	return 0;
}

static int print_name_stat_handler(pid_t pid,
				   const struct task_stat *stat,
				   void *data)
{
	printf("%lu:%d=%s\n", *((size_t *) data), pid, stat->name);
	return 0;
}

static int print_tid_handler(pid_t pid, tid_t tid, void *data)
{
	int ret = for_tid_stat(pid, tid, print_core_stat_handler, data);
	
	if (ret != 0)
		warning("cannot scan %d:%d", pid, tid);
	return 0;
}


static int track_stat_handler(pid_t pid,
			      const struct task_stat *stat,
			      void *data)
{
	size_t i;
	char child = 0;

	for (i=0; i < pids_length; i++) {
		if (pid == pids_to_scan[i])
			return 0;
		if (stat->ppid == pids_to_scan[i])
			child = 1;
	}

	if (child && print_name)
		printf("%lu:%d=%s\n", *((size_t *) data), pid, stat->name);
	if (child)
		track_pid(pid);

	return 0;
}

static int track_pid_handler(pid_t pid, void *data)
{
	for_pid_stat(pid, track_stat_handler, data);
	return 0;
}


static size_t now_millis(void)
{
	struct timespec ts;
	clock_gettime(CLOCK_REALTIME, &ts);
	return ts.tv_sec * 1000 + ts.tv_nsec / 1000000ul;
}

static void sleep_millis(size_t millis)
{
	struct timespec req, rem;
	int ret;

	req.tv_sec = millis / 1000;
	req.tv_nsec = (millis % 1000) * 1000000ul;

	ret = nanosleep(&req, &rem);
	while (ret != 0)
		ret = nanosleep(&rem, &rem);
}


static void parse_options(int *_argc, char ***_argv)
{
	int c, idx, argc = *_argc;
	char **argv = *_argv;
	char *err;
	static struct option options[] = {
		{"help",      no_argument,       0, 'h'},
		{"version",   no_argument,       0, 'V'},
		{"period",    required_argument, 0, 'p'},
		{"children",  optional_argument, 0, 'c'},
		{"name",      no_argument,       0, 'n'},
		{ NULL,       0,                 0,  0}
	};

	opterr = 0;

	while (1) {
		c = getopt_long(argc, argv, "hVp:cn", options, &idx);
		if (c == -1)
			break;

		switch (c) {
		case 'h':
			usage();
			exit(EXIT_SUCCESS);
		case 'V':
			version();
			exit(EXIT_SUCCESS);
		case 'p':
			scan_every_ms = strtol(optarg, &err, 10);
			if (*err != '\0')
				error("invalid period: '%s'", optarg);
			if (scan_every_ms == 0)
				error("invalid period: '%s'", optarg);
			break;
		case 'c':
			if (optarg == NULL) {
				children = default_children;
			} else {
				children = strtol(optarg, &err, 10);
				if (*err != '\0')
					error("invalid children: '%s'",optarg);
				if (children == 0)
					error("invalid children: '%s'",optarg);
			}
			break;
		case 'n':
			print_name = 1;
			break;
		default:
			error("unknown option '%s'", argv[optind-1]);
		}
	}

	*_argc -= optind;
	*_argv += optind;
}

static void parse_arguments(int argc, char **argv)
{
	char *err;
	pid_t pid;
	size_t time = 0;
	
	if (argc < 1)
		error("missing pid argument");
	if (argc > 1)
		error("unexpected argument '%s'", argv[1]);

	pid = strtol(argv[0], &err, 10);
	if (*err != '\0')
		error("invalid pid operand: '%s'", argv[0]);

	for_pid_stat(pid, print_name_stat_handler, &time);
	track_pid(pid);
}

int main(int argc, char **argv)
{
	size_t start, current, next;
	size_t i, step;
	int ret;
	
	progname = argv[0];
	parse_options(&argc, &argv);
	parse_arguments(argc, argv);

	signal(SIGTERM, signal_exit);
	signal(SIGINT, signal_exit);

	start = now_millis();
	next = start;

	step = 0;
	while (1) {
		current = now_millis();
		current_time = current - start;

		if (step == 0)
			foreach_pid(track_pid_handler, &current_time);

		for (i=0; i < pids_length; i++) {
			ret = foreach_tid(pids_to_scan[i], print_tid_handler,
					  &current_time);
			if (ret != 0)
				untrack_pid(i);

			if (pids_length == 0)
				clean_exit();
		}

		if (++step >= children)
			step = 0;

		while (next <= current)
			next += scan_every_ms;
		sleep_millis(next - current);
	}

	/* dead code */
	return EXIT_SUCCESS;
}
