/**
 * @file
 * @brief       Video utilities
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
 * Use `video_open()` to open and initialize video device.
 * Then you may use `video_read()` to get video frames out of the device.
 * @note `video_read()` is a blocking call
 *
 * Example:
 * @code
 * int main()
 * {
 *     video_t *video = video_open("/dev/video0", 800, 600, "RGB4", false);
 *     void *buffer;
 *     size_t length;
 *
 *     while(video_read(video, &buffer, &length))
 *     {
 *         // TODO: Do some processing here
 *     }
 *
 *     video_close(video);
 * }
 * @endcode
 */

#ifndef VIDEO_H
#define VIDEO_H

#include <stdint.h>
#include <stdbool.h>

/**
 * @brief Internal object
 */
typedef struct _video video_t;

/**
 * @brief Opens video device and starts the capture
 * @param device Device name eg. "/dev/video0"
 * @param width Video frame width in pixels
 * @param height Video frame height in pixels
 * @param format Video format in fourcc notation eg. "RGB4"
 * @param interlace Video interlacing (0 - disabled, 1 - enabled)
 * @return Video object or NULL on error
 */
video_t *video_open(const char *device, uint32_t width, uint32_t height, const char format[4], bool interlace);

/**
 * @brief Synchronously reads next video frame
 * @param video Object returned by `video_open()`
 * @param[out] data Pointer to pixel data buffer
 * @param[out] length Pointer to data length
 * @return 1 on success or 0 on error
 * @note Each call to `video_read()` invalidates previous buffer.
 */
int video_read(video_t *video, void **data, size_t *length);

/**
 * @brief Stops the capture and frees all resources
 * @param video Object returned by `video_open()`
 */
void video_close(video_t *video);

#endif /* VIDEO_H */
