/*
 * Video utilities
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

#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <linux/videodev2.h>

#include "debug.h"
#include "video.h"

// Requested buffer count
#define BUFFER_COUNT    4

struct _video
{
    // Device file descriptor
    int fd;

    // Frame buffers
    struct
    {
        void *start;
        size_t length;
    }
    buffers[BUFFER_COUNT];

    // Number of buffers mapped
    size_t count;

    // Index of currently processed buffer
    int index;
};

video_t *video_open(const char *device, uint32_t width, uint32_t height, const char format[4], bool interlace)
{
    DEBUG("video_open()");
    assert(device != 0);
    assert(format != 0);

    int i;
    video_t *video = calloc(1, sizeof(struct _video));
    assert(video != 0);

    // Open device
    if((video->fd = open(device, O_RDWR | O_NONBLOCK)) == -1)
    {
        WARN("Failed to open `%s`", device);
        free(video);
        return NULL;
    }

    struct v4l2_format fmt;
    bzero(&fmt, sizeof(fmt));
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt.fmt.pix.width = width;
    fmt.fmt.pix.height = height;
    fmt.fmt.pix.pixelformat = v4l2_fourcc(format[0], format[1], format[2], format[3]);
    fmt.fmt.pix.field = interlace ? V4L2_FIELD_INTERLACED : V4L2_FIELD_NONE;

    // Set video format
    if(ioctl(video->fd, VIDIOC_S_FMT, &fmt) == -1)
    {
        WARN("Failed to set video format");
        goto error;
    }

    struct v4l2_requestbuffers reqbuf;
    bzero(&reqbuf, sizeof(reqbuf));
    reqbuf.count = BUFFER_COUNT;
    reqbuf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    reqbuf.memory = V4L2_MEMORY_MMAP;

    // Request buffers
    if(ioctl(video->fd, VIDIOC_REQBUFS, &reqbuf) == -1)
    {
        WARN("Failed to request buffers");
        goto error;
    }
    assert(reqbuf.count <= BUFFER_COUNT);

    struct v4l2_buffer buf;
    for(i = 0; i < reqbuf.count; i++)
    {
        bzero(&buf, sizeof(buf));
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = i;

        // Query buffer
        if(ioctl(video->fd, VIDIOC_QUERYBUF, &buf) == -1)
        {
            WARN("Failed to query buffers");
            goto error;
        }

        // Map buffer
        video->buffers[i].length = buf.length;
        video->buffers[i].start = mmap(NULL, buf.length, PROT_READ | PROT_WRITE, MAP_SHARED, video->fd, buf.m.offset);
        if(video->buffers[i].start == MAP_FAILED)
        {
            WARN("Failed to map buffer");
            goto error;
        }
        else video->count++;
    }

    for(i = 1; i < video->count; i++)
    {
        bzero(&buf, sizeof(buf));
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = i;

        // Enqueue buffer
        if(ioctl(video->fd, VIDIOC_QBUF, &buf) == -1)
        {
            WARN("Failed to enqueue buffer");
            goto error;
        }
    }

    // Start capture
    enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if(ioctl(video->fd, VIDIOC_STREAMON, &type) == -1)
    {
        WARN("Failed to start stream");
        goto error;
    }

    INFO("Capture started with %d buffers", video->count);
    return video;

error:
    for(i = 0; video->count; i++)
    {
        // Unmap buffer
        munmap(video->buffers[i].start, video->buffers[i].length);
    }
    close(video->fd);
    free(video);
    return NULL;
}

int video_read(video_t *video, void **data, size_t *length)
{
    DEBUG("video_read()");
    assert(video != 0);

    struct v4l2_buffer buf;
    bzero(&buf, sizeof(buf));
    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;
    buf.index = video->index;

    // Enqueue previous buffer
    if(ioctl(video->fd, VIDIOC_QBUF, &buf) == -1)
    {
        WARN("Failed to enqueue buffer");
        return 0;
    }

    // Block until new data is ready
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(video->fd, &fds);
    select(video->fd + 1, &fds, NULL, NULL, NULL);
    bzero(&buf, sizeof(buf));
    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;

    // Dequeue next buffer
    if(ioctl(video->fd, VIDIOC_DQBUF, &buf) == -1)
    {
        WARN("Failed to dequeue buffer");
        return 0;
    }
    assert(buf.index < BUFFER_COUNT);

    INFO("Dequeued buffer %d of size %d", buf.index, buf.bytesused);
    if(data) *data = video->buffers[video->index = buf.index].start;
    if(length) *length = buf.bytesused;
    return 1;
}

void video_close(video_t *video)
{
    DEBUG("video_close()");
    assert(video != 0);

    // Stop capture
    enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if(ioctl(video->fd, VIDIOC_STREAMOFF, &type) == -1)
    {
        WARN("Failed to stop stream");
    }

    int i;
    for(i = 0; i < video->count; i++)
    {
        // Unmap buffer
        munmap(video->buffers[i].start, video->buffers[i].length);
    }

    close(video->fd);
    free(video);
}
