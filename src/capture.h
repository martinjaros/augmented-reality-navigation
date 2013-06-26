/**
 * @file
 * @brief       V4L2 utilities
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
 * This is a utility library for simple usage of Linux video devices (V4L2).
 * Use `capture_start()` to initialize the device, this returns file descriptor
 * on which you can `select()` or `poll()`. Buffers are allocated by kernel
 * and exchanged by pull / push mechanism, using indexes to the `buffers` array.
 * @note All functions do not block
 *
 * Example:
 * @code
 * static void process_buffer(void *buf, size_t len)
 * {
 *     // TODO: Do some processing here
 * }
 *
 * int main()
 * {
 *     struct buffer *buffers;
 *     int fd = capture_start("/dev/video0", 800, 600, "RGB4", false, &buffers);
 *
 *     while(1)
 *     {
 *         fd_set fds;
 *         FD_ZERO(&fds);
 *         FD_SET(fd, &fds);
 *
 *         select(fd + 1, &fds, NULL, NULL, NULL);
 *
 *         size_t bytesused;
 *         int index = capture_pop(fd, &bytesused);
 *
 *         process_buffer(buffers[index].start, bytesused);
 *
 *         capture_push(fd, index);
 *     }
 *
 *     capture_stop(fd, buffers);
 * }
 * @endcode
 */

#ifndef CAPTURE_H
#define CAPTURE_H

#include <stdint.h>
#include <stdbool.h>

/**
 * @brief Video frame buffer
 */
struct buffer
{
    /**
     * @brief Pointer to start of the buffer
     */
    void *start;

    /**
     * @brief Maximum size of the buffer
     */
    size_t length;
};

/**
 * @brief Opens video device and starts the capture
 * @param devname Device name eg. "/dev/video0"
 * @param width Video frame width in pixels
 * @param height Video frame height in pixels
 * @param format Video format in fourcc notation eg. "RGB4"
 * @param interlace Video interlacing (0 - disabled, 1 - enabled)
 * @param[out] buffers Pointer to a NULL terminated buffer array, allocated by driver
 * @return File descriptor of the opened device or -1 on error
 * @warning You should access only the buffer which index was previously popped from the queue
 */
int capture_start(const char *devname, uint32_t width, uint32_t height, const char format[4], bool interlace, struct buffer **buffers);

/**
 * @brief Pops next buffer from the queue (you should use `select()` to wait for the buffer)
 * @param fd File descriptor as returned by `capture_start()`
 * @param[out] bytesused Number of bytes in the buffer, set by driver
 * @return Index of the popped buffer or -1 on error
 */
int capture_pop(int fd, size_t *bytesused);

/**
 * @brief Pushes previously popped buffer back to the capture queue
 * @param fd File descriptor as returned by `capture_start()`
 * @param index Buffer index as returned by `capture_pop()`
 * @warning You should not access the buffer once it was returned to the kernel
 */
void capture_push(int fd, int index);

/**
 * @brief Stops the capture and frees all resources
 * @param fd File descriptor as returned by `capture_start()`
 * @param buffers Pointer to buffer array as set by `capture_start()`
 */
void capture_stop(int fd, struct buffer *buffers);

#endif /* CAPTURE_H */
