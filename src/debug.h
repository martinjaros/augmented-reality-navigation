/**
 * @file
 * @brief       Debugging utilities
 * @author      Martin Jaros <xjaros32@stud.feec.vutbr.cz>
 *
 * @section LICENSE
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
 *
 * @section DESCRIPTION
 * These are macros used for code debugging and tracing,
 * they print messages according to TRACE_LEVEL setting.
 * - TRACE_LEVEL 0 - (disabled)
 * - TRACE_LEVEL 1 - ERROR
 * - TRACE_LEVEL 2 - WARN
 * - TRACE_LEVEL 3 - INFO
 * - TRACE_LEVEL 4 - DEBUG
 */

#ifndef DEBUG_H
#define DEBUG_H

//! @cond
void _debug_printf(int level, const char *file, int line, const char *msg, ...);
//! @endcond

#if TRACE_LEVEL > 0
#define ERROR(msg, ...) _debug_printf(1, __FILE__, __LINE__, msg, ##__VA_ARGS__)
#else
#define ERROR(msg, ...)
#endif

#if TRACE_LEVEL > 1
#define WARN(msg, ...) _debug_printf(2, __FILE__, __LINE__, msg, ##__VA_ARGS__)
#else
#define WARN(msg, ...)
#endif

#if TRACE_LEVEL > 2
#define INFO(msg, ...) _debug_printf(3, __FILE__, __LINE__, msg, ##__VA_ARGS__)
#else
#define INFO(msg, ...)
#endif

#if TRACE_LEVEL > 3
#define DEBUG(msg, ...) _debug_printf(4, __FILE__, __LINE__, msg, ##__VA_ARGS__)
#else
#define DEBUG(msg, ...)
#endif

#endif /* DEBUG_H */
