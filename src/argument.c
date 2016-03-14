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

#include <ctype.h>
#include <sched.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>


static size_t      next_mask;
static size_t      total_masks = 0;
static cpu_set_t  *all_masks;

static size_t      total_map = 0;
static size_t     *map_forward;
static size_t     *map_reverse;


static void *inner_malloc(size_t len)
{
	void *addr = mmap(NULL, len, PROT_READ | PROT_WRITE,
			  MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

	if (addr == MAP_FAILED)
		return NULL;
	return addr;
}

static size_t count_words(const char *arg)
{
	size_t count = 0;

	while (*arg != '\0') {
		if (isspace(*arg)) {
			arg++;
			continue;
		}

		count++;
		while (*arg != '\0' && !isspace(*arg))
			arg++;
	}

	return count;
}

static const char *next_word(const char *str, const char **word)
{
	const char *wrd;

	if (word == NULL)
		word = &wrd;
	
	while (isspace(*str))
		str++;
	
	if (*str == '\0') {
		*word = NULL;
		return str;
	}

	*word = str;
	while (*str != '\0' && !isspace(*str))
		str++;
	
	return str;
}


static int parse_cpumask(cpu_set_t *dest, const char *word, size_t len)
{
	char *buffer = alloca(len + 1);
	long i, start, end, swap;
	char *ptr;

	memcpy(buffer, word, len);
	buffer[len] = '\0';
	
	CPU_ZERO(dest);

	while (1) {
		start = strtol(buffer, &ptr, 10);
		buffer = ptr;

		if (*buffer == '-') {
			buffer++;
			end = strtol(buffer, &ptr, 10);
			buffer = ptr;
		} else {
			end = start;
		}

		if (start > end) {
			swap = start;
			start = end;
			end = swap;
		}

		for (i=start; i <= end; i++)
			CPU_SET(i, dest);
		
		if (*buffer != ',')
			break;
		buffer++;
	}

	if (*buffer != '\0')
		return -1;
	return 0;
}


static void acquire_round_robin(const char *arg, const char *argname)
{
	size_t count = 0, total = count_words(arg);
	cpu_set_t *masks = inner_malloc(sizeof (cpu_set_t) * total);
	const char *word;
	int err;

	if (masks == NULL)
		errorp("failed to parse '%s' = '%s'", argname, arg);

	arg = next_word(arg, &word);
	while (word != NULL) {
		err = parse_cpumask(masks + count, word, arg - word);
		if (err != 0)
			error("failed to parse '%s' = '%s'", argname, arg);
		
		arg = next_word(arg, &word);
		count++;
	}

	free(all_masks);
	
	next_mask = 0;
	all_masks = masks;
	total_masks = total;
}


static int parse_mapping(size_t *from, size_t *to, const char *word, size_t l)
{
	char *buffer = alloca(l + 1);
	char *ptr, *err;

	memcpy(buffer, word, l);
	buffer[l] = '\0';

	*from = strtol(buffer, &err, 10);
	if (*err != '=')
		return -1;

	ptr = err + 1;
	*to = strtol(ptr, &err, 10);
	if (*err != '\0')
		return -1;

	return 0;
}

static int compute_map(size_t *froms, size_t *tos, size_t len)
{
	size_t *forward, *reverse;
	size_t i, max, total;

	max = 0;
	for (i=0; i<len; i++) {
		if (froms[i] > max)
			max = froms[i];
		if (tos[i] > max)
			max = tos[i];
	}
	total = max + 1;

	if ((forward = inner_malloc(sizeof (size_t) * total)) == NULL)
		return -1;
	if ((reverse = inner_malloc(sizeof (size_t) * total)) == NULL)
		return -1;

	for (i=0; i<total; i++) {
		forward[i] = i;
		reverse[i] = i;
	}

	for (i=0; i<len; i++) {
		forward[froms[i]] = tos[i];
		reverse[tos[i]] = froms[i];
	}

	total_map = total;
	map_forward = forward;
	map_reverse = reverse;

	return 0;
}

static void acquire_map(const char *arg, const char *argname)
{
	size_t count = 0, total = count_words(arg);
	const char *word, *origin = arg;
	size_t *froms, *tos;
	int err;

	if ((froms = malloc(sizeof (size_t) * total)) == NULL)
		errorp("failed to parse '%s' = '%s'", argname, arg);
	if ((tos = malloc(sizeof (size_t) * total)) == NULL)
		errorp("failed to parse '%s' = '%s'", argname, arg);

	arg = next_word(arg, &word);
	while (word != NULL) {
		err = parse_mapping(froms+count, tos+count, word, arg - word);
		if (err != 0)
			error("failed to parse '%s' = '%s'", argname, arg);

		arg = next_word(arg, &word);
		count++;
	}

	if (compute_map(froms, tos, count) != 0)
		error("failed to parse '%s' = '%s'", argname, origin);

	free(froms);
	free(tos);
}


void acquire_arguments(void)
{
	char *arg;

	arg = getenv("PIN_MAP");
	if (arg != NULL)
		acquire_map(arg, "PIN_MAP");

	arg = getenv("PIN_RR");
	if (arg != NULL)
		acquire_round_robin(arg, "PIN_RR");
}
	
const cpu_set_t *get_next_cpumask(void)
{
	size_t id;
	size_t old, new;

	if (total_masks == 0)
		return NULL;

	id = __sync_fetch_and_add(&next_mask, 1);
	do {
		old = next_mask;
		if (old < total_masks)
			break;
		new = old % total_masks;
	} while (__sync_bool_compare_and_swap(&next_mask, old, new));

	if (id >= total_masks)
		id = id % total_masks;

	return all_masks + id;
}


static void map_cpuset(cpu_set_t *dest, const cpu_set_t *src, size_t len,
			size_t *translate)
{
	size_t i, count = CPU_COUNT_S(len, src);
	size_t size = len << 3;

	CPU_ZERO(dest);

	for (i=0; i<size && count; i++) {
		if (count == 0)
			break;
		if (!CPU_ISSET_S(i, len, src))
			continue;

		if (i < total_map)
			CPU_SET_S(translate[i], len, dest);
		else
			CPU_SET_S(i, len, dest);

		count--;
	}
}

void map_cpuset_forward(cpu_set_t *dest, const cpu_set_t *src, size_t len)
{
	map_cpuset(dest, src, len, map_forward);
}

void map_cpuset_reverse(cpu_set_t *dest, const cpu_set_t *src, size_t len)
{
	map_cpuset(dest, src, len, map_reverse);
}

int map_cpu_reverse(int cpu)
{
	if (cpu < 0)
		return cpu;
	if ((size_t) cpu >= total_map)
		return cpu;
	return map_reverse[cpu];
}
