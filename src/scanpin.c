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


#define SLURP_CHUNK 256
#define PIDS_CHUNK  16

#define LASTCPU_NUMFIELD 37

#define PID_MAXLEN 5
#define TID_MAXLEN 5

#define TASKS_PATH_PATTERN  "/proc/%d/task/"
#define TASKS_PATH_MAXLEN   (12 + PID_MAXLEN)

#define STAT_PATH_PATTERN   "/proc/%d/task/%s/stat"
#define STAT_PATH_MAXLEN    (17 + PID_MAXLEN + TID_MAXLEN)

#define CHLD_PATH_PATTERN   "/proc/%d/task/%s/children"
#define CHLD_PATH_MAXLEN    (21 + PID_MAXLEN + TID_MAXLEN)


const char *progname;

char    children = 0;
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
	       "  -p, --period [ms]      Collect information every specified "
	       "amount of\n"
	       "                         millisecond [default = %lu]\n"
	       "  -c, --children         Collect information for children (and descendants) too\n",
	       scan_every_ms);
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


static void track_pid(pid_t pid)
{
	size_t i;

	for (i=0; i < pids_length; i++)
		if (pids_to_scan[i] == pid)
			return;

	if (pids_length == pids_capacity) {
		pids_capacity += PIDS_CHUNK;
		pids_to_scan = realloc(pids_to_scan, sizeof (pid_t)
				       * pids_capacity);
		if (pids_to_scan == NULL)
			error("memory allocation failed for %lu",
			      sizeof (pid_t) * pids_capacity);
	}

	pids_to_scan[pids_length++] = pid;
}

static void untrack_pid(pid_t pid)
{
	size_t i;

	for (i=0; i < pids_length; i++)
		if (pids_to_scan[i] == pid) {
			pids_to_scan[i] = pids_to_scan[--pids_length];
			return;
		}
}


static char *slurp(FILE *stream)
{
	size_t capacity = SLURP_CHUNK;
	char *buffer = malloc(capacity + 1);
	size_t length = 0;

	while (!feof(stream)) {
		if (length == capacity) {
			capacity += SLURP_CHUNK;
			buffer = realloc(buffer, capacity + 1);
			if (buffer == NULL)
				error("memory allocation failed for %lu",
				      capacity);
			buffer[capacity] = '\0';
		}

		length += fread(buffer + length, 1, capacity - length, stream);
	}

	buffer[length] = '\0';

	return buffer;
}

static void erase_program_name(char *content)
{
	char *open_bracket = NULL;
	char *close_bracket = NULL;
	char *ptr;

	for (ptr = content; *ptr != '\0'; ptr++) {
		if (*ptr == '(' && open_bracket == NULL)
			open_bracket = ptr;
		if (*ptr == ')')
			close_bracket = ptr;
	}

	if (open_bracket == NULL || close_bracket <= open_bracket)
		error("invalid stat format: '%s'", content);

	while (open_bracket <= close_bracket) {
		*open_bracket = ' ';
		open_bracket++;
	}
}

static size_t get_field(const char *content, size_t field)
{
	const char *ptr = content;
	size_t cur = 0;
	
	while (*ptr != '\0') {
		while (*ptr == ' ')
			ptr++;
		if (cur == field)
			break;
		
		cur++;
		while (*ptr != ' ')
			ptr++;
	}

	return ptr - content;
}

static void collect_task(pid_t pid, const char *tid)
{
	char buffer[STAT_PATH_MAXLEN + 1];
	char *content, *ptr, *err;
	size_t core;
	FILE *stat;

	snprintf(buffer, sizeof (buffer), STAT_PATH_PATTERN, pid, tid);
	stat = fopen(buffer, "r");
	if (stat == NULL) {
		warning("cannot open '%s'", buffer);
		return;
	}

	content = slurp(stat);
	erase_program_name(content);  /* the program name is hard to parse */

	ptr = content + get_field(content, LASTCPU_NUMFIELD);
	core = strtol(ptr, &err, 10);
	if (*err != ' ' && *err != '\n')
		error("invalid stat format '%s'", content);
	printf("%lu:%d:%s:%lu\n", current_time, pid, tid, core);

	free(content);
	fclose(stat);
}

static pid_t next_child(const char *content, size_t *from)
{
	const char *ptr = content + *from;
	pid_t pid = 0;
	char *err;

	while (*ptr == ' ')
		ptr++;
	if (*ptr == '\0')
		goto out;

	pid = strtol(ptr, &err, 10);
	if (*err != ' ' && *err != '\0')
		error("invalid children format '%s'", *content);

 out:
	*from = err - content;
	return pid;
}

static void collect_children(pid_t pid, const char *tid)
{
	char buffer[CHLD_PATH_MAXLEN + 1];
	FILE *children;
	char *content;
	size_t index;
	pid_t child;

	snprintf(buffer, sizeof (buffer), CHLD_PATH_PATTERN, pid, tid);
	children = fopen(buffer, "r");
	if (children == NULL) {
		warning("cannot open '%s'", buffer);
		return;
	}

	content = slurp(children);

	index = 0;
	while ((child = next_child(content, &index)) != 0)
		track_pid(child);

	free(content);
	fclose(children);
}

static void collect_process(pid_t pid)
{
	char buffer[TASKS_PATH_MAXLEN + 1];
	struct dirent *entry;
	DIR *tasks;

	snprintf(buffer, sizeof (buffer), TASKS_PATH_PATTERN, pid);
	tasks = opendir(buffer);
	if (tasks == NULL) {
		untrack_pid(pid);
		if (pids_length == 0)
			clean_exit();
		return;
	}

	while ((entry = readdir(tasks)) != NULL) {
		if (entry->d_name[0] == '.')
			continue;
		collect_task(pid, entry->d_name);
		if (children)
			collect_children(pid, entry->d_name);
	}

	closedir(tasks);
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
		{"chilren",   required_argument, 0, 'c'},
		{ NULL,       0,                 0,  0}
	};

	opterr = 0;

	while (1) {
		c = getopt_long(argc, argv, "hVp:c", options, &idx);
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
			children = 1;
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
	
	if (argc < 1)
		error("missing pid argument");
	if (argc > 1)
		error("unexpected argument '%s'", argv[1]);

	pid = strtol(argv[0], &err, 10);
	if (*err != '\0')
		error("invalid pid operand: '%s'", argv[0]);

	track_pid(pid);
}

int main(int argc, char **argv)
{
	size_t start, current, next;
	size_t i, len;
	
	progname = argv[0];
	parse_options(&argc, &argv);
	parse_arguments(argc, argv);

	signal(SIGTERM, signal_exit);
	signal(SIGINT, signal_exit);

	start = now_millis();
	current = start;
	next = start;
	
	while (1) {
		while (next <= current)
			next += scan_every_ms;
		sleep_millis(next - current);

		current = now_millis();
		current_time = current - start;

		len = pids_length;
		for (i=0; i < len; i++)
			collect_process(pids_to_scan[i]);
	}

	/* dead code */
	return EXIT_SUCCESS;
}
