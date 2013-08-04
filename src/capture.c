/*
 * V4L2 utilities
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
#include "capture.h"

/* Resource cleanup helper function */
static void cleanup(int fd, struct buffer *buffers)
{
    if(buffers != NULL)
    {
        int i;
        for (i = 0; (buffers[i].start != NULL) && (buffers[i].start != MAP_FAILED); i++)
        {
            // Unmap buffer
            munmap(buffers[i].start, buffers[i].length);
        }
        free(buffers);
    }

    close(fd);
}

int capture_start(const char *devname, uint32_t width, uint32_t height, const char format[4], bool interlace, struct buffer **buffers)
{
    DEBUG("capture_start()");
    int fd, i;

    assert(devname != NULL);
    assert(format != NULL);
    assert(buffers != NULL);
    *buffers = NULL;

    // Open device
    if((fd = open(devname, O_RDWR | O_NONBLOCK)) == -1)
    {
        WARN("Failed to open `%s`", devname);
        return fd;
    }

    struct v4l2_format fmt;
    bzero(&fmt, sizeof(fmt));
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt.fmt.pix.width = width;
    fmt.fmt.pix.height = height;
    fmt.fmt.pix.pixelformat = v4l2_fourcc(format[0], format[1], format[2], format[3]);
    fmt.fmt.pix.field = interlace ? V4L2_FIELD_INTERLACED : V4L2_FIELD_NONE;

    // Set capture format
    if(ioctl(fd, VIDIOC_S_FMT, &fmt) == -1)
    {
        WARN("Failed to set video format");
        cleanup(fd, *buffers);
        return -1;
    }

    struct v4l2_requestbuffers reqbuf;
    bzero(&reqbuf, sizeof(reqbuf));
    reqbuf.count = 4;
    reqbuf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    reqbuf.memory = V4L2_MEMORY_MMAP;

    // Request buffers
    if(ioctl(fd, VIDIOC_REQBUFS, &reqbuf) == -1)
    {
        WARN("Failed to request buffers");
        cleanup(fd, *buffers);
        return -1;
    }

    // Allocate buffers
    *buffers = calloc(reqbuf.count + 1, sizeof(struct buffer));
    if(*buffers == NULL)
    {
        WARN("Cannot allocate memory");
        cleanup(fd, *buffers);
        return -1;
    }

    struct v4l2_buffer buf;
    for(i = 0; i < reqbuf.count; i++)
    {
        bzero(&buf, sizeof(buf));
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = i;

        // Query buffer
        if(ioctl(fd, VIDIOC_QUERYBUF, &buf) == -1)
        {
            WARN("Failed to query buffers");
            cleanup(fd, *buffers);
            return -1;
        }

        // Map buffer
        (*buffers)[i].length = buf.length;
        (*buffers)[i].start = mmap(NULL, buf.length, PROT_READ | PROT_WRITE, MAP_SHARED, fd, buf.m.offset);
        if((*buffers)[i].start == MAP_FAILED)
        {
            WARN("Failed to map buffer");
            cleanup(fd, *buffers);
            return -1;
        }
    }

    for(i = 0; i < reqbuf.count; i++)
    {
        bzero(&buf, sizeof(buf));
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = i;

        // Push buffer
        if(ioctl(fd, VIDIOC_QBUF, &buf) == -1)
        {
            WARN("Failed to queue buffer");
            cleanup(fd, *buffers);
            return -1;
        }
    }

    // Start video capture
    enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if(ioctl(fd, VIDIOC_STREAMON, &type) == -1)
    {
        WARN("Failed to start stream");
        cleanup(fd, *buffers);
        return -1;
    }

    return fd;
}

int capture_pop(int fd, size_t *len)
{
    DEBUG("capture_pop()");
    assert(fd >= 0);
    assert(len != NULL);

    struct v4l2_buffer buf;
    bzero(&buf, sizeof(buf));
    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;

    // Dequeue buffer
    if(ioctl(fd, VIDIOC_DQBUF, &buf) == -1)
    {
        WARN("Failed to dequeue buffer");
        *len = 0;
        return -1;
    }

    *len = buf.bytesused;
    return buf.index;
}

void capture_push(int fd, int index)
{
    DEBUG("capture_push()");
    assert(fd >= 0);
    assert(index > 0);

    struct v4l2_buffer buf;
    bzero(&buf, sizeof(buf));
    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;
    buf.index = index;

    // Push buffer
    if(ioctl(fd, VIDIOC_QBUF, &buf) == -1)
    {
        WARN("Failed to queue buffer");
    }
}

void capture_stop(int fd, struct buffer *buffers)
{
    DEBUG("capture_stop()");
    assert(fd >= 0);

    // Stop video capture
    enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if(ioctl(fd, VIDIOC_STREAMOFF, &type) == -1)
    {
        WARN("Failed to stop stream");
    }

    cleanup(fd, buffers);
}
