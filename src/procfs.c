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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "procfs.h"


#define SLURP_CHUNK  4096

#define TASK_STAT_PATH_PATTERN  "/proc/%d/task/%d/stat"
#define TASK_STAT_PATH_MAXLEN   (17 + PID_MAXLEN + TID_MAXLEN)

#define STAT_PATH_PATTERN       "/proc/%d/stat"
#define STAT_PATH_MAXLEN        (11 + PID_MAXLEN)

#define TASK_PATH_PATTERN       "/proc/%d/task"
#define TASK_PATH_MAXLEN        (11 + PID_MAXLEN)


static char *slurp(FILE *stream)
{
	size_t capacity = SLURP_CHUNK;
	char *buffer = malloc(capacity + 1);
	size_t length = 0;
	char *nbuffer;

	while (!feof(stream)) {
		if (length == capacity) {
			capacity += SLURP_CHUNK;
			nbuffer = realloc(buffer, capacity + 1);
			if (nbuffer == NULL) {
				free(buffer);
				return NULL;
			}

			buffer = nbuffer;
			buffer[capacity] = '\0';
		}

		length += fread(buffer + length, 1, capacity - length, stream);
	}

	buffer[length] = '\0';

	return buffer;
}

static char *pslurp(const char *path)
{
	FILE *fh = fopen(path, "r");
	char *content;
	int ret;

	if (fh == NULL)
		return NULL;
	
	content = slurp(fh);
	ret = fclose(fh);

	if (content == NULL || ret != 0) {
		free(content);
		return NULL;
	}

	return content;
}


static int parse_stat(struct task_stat *dest, char *raw)
{
	char *ptr;
	size_t i;

	/* PID */
	dest->pid = strtol(raw, &ptr, 10);
	if (*ptr != ' ')
		return -1;
	raw = ptr;

	while (*raw == ' ')
		raw++;

	/* NAME */
	if (*raw != '(')
		return -1;
	ptr = strrchr(raw, ')');
	if (ptr == NULL)
		return -1;
	raw++;
	*ptr = '\0';
	dest->name = raw;
	raw = ptr + 1;

	while (*raw == ' ')
		raw++;

	/* STATE */
	dest->state = *raw++;

	while (*raw == ' ')
		raw++;

	/* PPID */
	dest->ppid = strtol(raw, &ptr, 10);
	if (*ptr != ' ')
		return -1;
	raw = ptr;

	for (i=0; i<34; i++) {
		while (*raw == ' ')
			raw++;
		while (*raw != ' ')
			raw++;
	}

	/* CORE */
	dest->core = strtol(raw, &ptr, 10);
	if (*ptr != ' ')
		return -1;
	raw = ptr;

	return 0;
}

int for_tid_stat(pid_t pid, tid_t tid,
		 int (*cb)(pid_t, tid_t, const struct task_stat *, void *),
		 void *data)
{
	char buffer[TASK_STAT_PATH_MAXLEN + 1];
	struct task_stat content;
	char *rawcontent;
	int ret;

	snprintf(buffer, sizeof (buffer), TASK_STAT_PATH_PATTERN, pid, tid);
	rawcontent = pslurp(buffer);
	if (rawcontent == NULL)
		return -1;
	
	ret = parse_stat(&content, rawcontent);
	if (ret == 0)
		ret = cb(pid, tid, &content, data);

	free(rawcontent);
	return ret;
}

int for_pid_stat(pid_t pid,
		 int (*cb)(pid_t, const struct task_stat *, void *),
		 void *data)
{
	char buffer[STAT_PATH_MAXLEN + 1];
	struct task_stat content;
	char *rawcontent;
	int ret;

	snprintf(buffer, sizeof (buffer), STAT_PATH_PATTERN, pid);
	rawcontent = pslurp(buffer);
	if (rawcontent == NULL)
		return -1;
	
	ret = parse_stat(&content, rawcontent);
	if (ret == 0)
		ret = cb(pid, &content, data);

	free(rawcontent);
	return ret;
}


int foreach_pid(int (*cb)(pid_t, void *), void *data)
{
	DIR *proc = opendir("/proc");
	struct dirent *entry;
	char *err;
	pid_t pid;
	int ret;

	if (proc == NULL)
		return -1;

	while ((entry = readdir(proc)) != NULL) {
		pid = strtol(entry->d_name, &err, 10);
		if (*err != '\0')
			continue;
		
		ret = cb(pid, data);
		if (ret != 0) {
			closedir(proc);
			return ret;
		}
	}

	return closedir(proc);
}

int foreach_tid(pid_t pid, int (*cb)(pid_t, tid_t, void *), void *data)
{
	char buffer[TASK_PATH_MAXLEN + 1];
	struct dirent *entry;
	DIR *task;
	char *err;
	tid_t tid;
	int ret;

	snprintf(buffer, sizeof (buffer), TASK_PATH_PATTERN, pid);
	task = opendir(buffer);
	if (task == NULL)
		return -1;

	while ((entry = readdir(task)) != NULL) {
		tid = strtol(entry->d_name, &err, 10);
		if (*err != '\0')
			continue;

		ret = cb(pid, tid, data);
		if (ret != 0) {
			closedir(task);
			return ret;
		}
	}

	return closedir(task);
}
