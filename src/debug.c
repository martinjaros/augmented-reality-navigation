/*
 * Debugging utilities
 *
 * Copyright (C) 2013 - Martin Jaros <xjaros32@stud.feec.vutbr.cz>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details at
 * <http://www.gnu.org/licenses>
 */

#include <stdio.h>
#include <unistd.h>
#include <assert.h>
#include <stdarg.h>
#include <sys/syscall.h>
#include <sys/time.h>

#include "debug.h"

#define BUFFER_SIZE     256

void _debug_printf(int level, const char *file, int line, const char *msg, ...)
{
    char buffer[BUFFER_SIZE];
    static const char *level_names[] =
    {
        "ERROR",
        "WARN",
        "INFO",
        "DEBUG"
    };

    struct timeval tv;
    gettimeofday(&tv, NULL);
    int offset = snprintf(buffer, BUFFER_SIZE, "%.2ld:%.2ld:%.2ld.%.3ld [0x%lx] (%s:%d) %s - ",
                          tv.tv_sec / 3600 % 24, tv.tv_sec / 60 % 60, tv.tv_sec % 60, tv.tv_usec / 1000, syscall(SYS_gettid), file, line, level_names[level - 1]);
    assert((offset > 0) && (offset < BUFFER_SIZE));

    va_list args;
    va_start(args, msg);
    vsnprintf(buffer + offset, BUFFER_SIZE - offset, msg, args);
    va_end(args);

    puts(buffer);
}
