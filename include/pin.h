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

#ifndef PIN_H
#define PIN_H


#define _GNU_SOURCE

#include <sched.h>


void warning(const char *format, ...);

void error(const char *format, ...);

void errorp(const char *format, ...);


void acquire_arguments(void);

const cpu_set_t *get_next_cpumask(void);

void map_cpuset_forward(cpu_set_t *dest, const cpu_set_t *src, size_t len);

void map_cpuset_reverse(cpu_set_t *dest, const cpu_set_t *src, size_t len);

int map_cpu_reverse(int cpu);


#endif
