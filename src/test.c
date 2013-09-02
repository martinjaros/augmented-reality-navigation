/*
 * ARNav test suite
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
#include <assert.h>
#include <string.h>
#include <unistd.h>

#include "debug.h"
#include "graphics.h"
#include "gps.h"
#include "imu.h"
#include "capture.h"

int test_graphics(const char *font, uint16_t width, uint16_t height, int loops)
{
    DEBUG("test_graphics()");

    assert(font != NULL);

    graphics_t *g = graphics_init(graphics_window_create(width, height));
    if(g == NULL)
    {
        ERROR("Cannot initialize graphics");
        return 0;
    }

    atlas_t *atlas = graphics_atlas_create(font, 36);
    if(atlas == NULL)
    {
        ERROR("Cannot create font atlas");
        graphics_free(g);
        return 0;
    }

    drawable_t *label = graphics_label_create(g, atlas);
    if(label == NULL)
    {
        ERROR("Cannot create label");
        graphics_atlas_free(atlas);
        graphics_free(g);
        return 0;
    }
    graphics_label_set_text(label, ANCHOR_LEFT_TOP, "Hello World");
    graphics_label_set_color(label, (uint8_t*)"\xFF\x00\x00\xFF");

    int i;
    for(i = 0; i < loops; i++)
    {
        graphics_draw(g, label, 10, 10);
        if(!graphics_flush(g, (uint8_t*)"\x7F\x7F\xFF\x00"))
        {
            WARN("Unable to draw");
            break;
        }
    }

    graphics_drawable_free(label);
    graphics_atlas_free(atlas);
    graphics_free(g);

    return 1;
}

int test_gps(const char *devname, int loops)
{
    DEBUG("test_gps()");

    assert(devname != NULL);

    int fd = gps_open(devname);
    if(fd < 0)
    {
        ERROR("Cannot initialize GPS device");
        return 0;
    }

    int i;
    for(i = 0; i < loops; i++)
    {
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(fd, &fds);

        DEBUG("select()");
        select(fd + 1, &fds, NULL, NULL, NULL);

        struct gpsdata data = { };
        gps_read(fd, &data);
    }

    gps_close(fd);

    return 1;
}

int test_imu(const char *devname, const char *calibname, int loops)
{
    DEBUG("test_imu()");
    assert(devname != NULL);
    assert(calibname != NULL);

    struct imucalib calib;
    if(!imu_calib_load(&calib, calibname))
    {
        ERROR("Cannot load IMU calibration");
        return 0;
    }

    imu_t *imu = imu_open(devname, &calib);
    if(imu == 0)
    {
        ERROR("Cannot initialize IMU device");
        return 0;
    }

    int i;
    for(i = 0; i < loops; i++)
    {
        usleep(1000000);

        struct imudata data = { };
        if(!imu_read(imu, &data))
        {
            ERROR("IMU read error");
            break;
        }
    }

    imu_close(imu);

    return 1;
}

int test_capture(const char *devname, uint16_t width, uint16_t height, int loops)
{
    DEBUG("test_capture()");

    assert(devname != NULL);

    graphics_t *g = graphics_init(graphics_window_create(width, height));
    if(g == NULL)
    {
        ERROR("Cannot initialize graphics");
        return 0;
    }

    drawable_t *image = graphics_image_create(g, width, height);
    if(image == NULL)
    {
        ERROR("Cannot create image");
        graphics_free(g);
        return 0;
    }

    struct buffer *buffers;
    int fd = capture_start(devname, width, height, "RGB4", false, &buffers);
    if(fd < 0)
    {
        ERROR("Cannot start video capture");
        graphics_drawable_free(image);
        graphics_free(g);
        return 0;
    }

    int i;
    for(i = 0; i < loops; i++)
    {
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(fd, &fds);

        DEBUG("select()");
        select(fd + 1, &fds, NULL, NULL, NULL);

        size_t bytesused;
        int index = capture_pop(fd, &bytesused);
        assert(bytesused >= 4 * width * height);
        graphics_image_set_bitmap(image, buffers[index].start);
        graphics_draw(g, image, 0, 0);
        if(!graphics_flush(g, NULL))
        {
            WARN("Unable to draw");
            break;
        }
        capture_push(fd, index);
    }

    capture_stop(fd, buffers);
    graphics_drawable_free(image);
    graphics_free(g);

    return 1;
}
