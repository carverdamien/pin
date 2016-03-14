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

#include <pin.h>

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


#define ERRMSG_HEADER  "pin: "
#define ERRMSG_FOOTER  "Please type 'man pin' for more informations\n"


void warning(const char *format, ...)
{
	va_list ap;

	fprintf(stderr, ERRMSG_HEADER);
	
	va_start(ap, format);
	vfprintf(stderr, format, ap);
	va_end(ap);

	fprintf(stderr, "\n" ERRMSG_FOOTER);
}

void error(const char *format, ...)
{
	va_list ap;

	fprintf(stderr, ERRMSG_HEADER);
	
	va_start(ap, format);
	vfprintf(stderr, format, ap);
	va_end(ap);

	fprintf(stderr, "\n" ERRMSG_FOOTER);
	abort();
}

void errorp(const char *format, ...)
{
	va_list ap;
	int errnum = errno;

	fprintf(stderr, ERRMSG_HEADER);
	
	va_start(ap, format);
	vfprintf(stderr, format, ap);
	va_end(ap);

	fprintf(stderr, ": %s\n" ERRMSG_FOOTER, strerror(errnum));
	abort();
}
