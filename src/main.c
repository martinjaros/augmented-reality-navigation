/*
 * Augmented reality navigation
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
#include <stdio.h>
#include <string.h>
#include <getopt.h>
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <math.h>
#include <sys/param.h>

#include "graphics.h"
#include "capture.h"
#include "gps.h"
#include "imu.h"
#include "nav.h"

#ifndef M_PI
#define M_PI    3.14159265358979323846
#endif

/* Helper macro, returns higher of the two arguments */
#define MAX(a,b) (((a)>(b))?(a):(b))

/* Video format */
#define VIDEO_WIDTH       800
#define VIDEO_HEIGHT      600
#define VIDEO_FORMAT      "RGB4"
#define VIDEO_INTERLACE   false

#define VIDEO_SIZE        4 * VIDEO_WIDTH * VIDEO_HEIGHT
#define VIDEO_HFOV        60.0
#define VIDEO_VFOV        60.0

/* Font format */
#define FONT_COLOR        { 0, 0, 0, 255 }
#define FONT_SIZE         12

/* Prints command line usage */
static void usage()
{
    printf("Augmented reality navigation\n"
           "usage: arnav [--help] [--video=<device>] [--gps=<device>] [--imu=<device>] [--wpts=<filename>] [--font=<fontname>]\n\n"
           "   --help              Shows help\n"
           "   --video=<device>    Sets video device to use (\"/dev/video*\")\n"
           "   --gps=<device>      Sets GPS device to use (\"/dev/tty*\")\n"
           "   --imu=<device>      Sets IMU device to use (\"/dev/i2c-*\")\n"
           "   --wpts=<filename>   Sets waypoint file to use\n"
           "   --font=<fontname>   Sets TrueType font file to use\n"
           "\n");
}

int main(int argc, char *argv[])
{
    int nfds = 0;
    char *videodev = "/dev/video0";
    char *gpsdev = "/dev/ttyS0";
    char *imudev = "/dev/i2c-0";
    char *wptf = "waypoints.txt";
    char *fontname = "FreeSans.ttf";

    int index = 0;
    static struct option options[] = {
        { "help",  no_argument,       0, 0 },
        { "video", required_argument, 0, 0 },
        { "gps",   required_argument, 0, 0 },
        { "imu",   required_argument, 0, 0 },
        { "wpts",  required_argument, 0, 0 },
        { "font",  required_argument, 0, 0 },
        { 0, 0, 0, 0}
    };

    // Parse options
    while (getopt_long(argc, argv, "", options, &index) == 0)
    {
        switch (index)
        {
            case 0: usage(); return EXIT_SUCCESS;
            case 1: videodev = optarg; break;
            case 2: gpsdev = optarg; break;
            case 3: imudev = optarg; break;
            case 4: wptf = optarg; break;
            case 5: fontname = optarg; break;
        }
    }

    // Initialize graphics
    graphics_t *gr = graphics_init(fontname, FONT_SIZE);
    if((gr == NULL))
    {
        fprintf(stderr, "Cannot initialize graphics\n");
        abort();
    }

    // Load waypoints
    wpt_iter *wpts = nav_load(wptf);
    if(wpts == NULL)
    {
        fprintf(stderr, "Cannot load %s\n", wptf);
        abort();
    }

    // Open GPS
    struct posinfo pos = { 0, 0, 0};
    int gpsfd = gps_open(gpsdev);
    if(gpsfd < 0)
    {
        fprintf(stderr, "Cannot open %s\n", gpsdev);
        abort();
    }
    nfds = MAX(gpsfd, nfds);

    // Open IMU
    struct attdinfo attd = { 0, 0, 0};
    imu_t *imu = imu_open(imudev);
    int imufd = imu_timer_create(imu);
    if(imufd < 0)
    {
        fprintf(stderr, "Cannot open %s\n", imudev);
        abort();
    }
    nfds = MAX(imufd, nfds);

    // Start capture
    struct buffer *buffers;
    int videofd = capture_start(videodev, VIDEO_WIDTH, VIDEO_HEIGHT, VIDEO_FORMAT, VIDEO_INTERLACE, &buffers);
    if(videofd < 0)
    {
        fprintf(stderr, "Cannot open %s\n", videodev);
        abort();
    }
    nfds = MAX(videofd, nfds);

    // Enter main loop
    while(1)
    {
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(gpsfd, &fds);
        FD_SET(imufd, &fds);
        FD_SET(videofd, &fds);
        select(nfds, &fds, NULL, NULL, NULL);

        // Process GPS
        if(FD_ISSET(gpsfd, &fds))
        {
            gps_read(gpsfd, &pos);
        }

        // Process IMU
        if(FD_ISSET(imufd, &fds))
        {
            imu_read(imu, &attd);
        }

        // Process video
        if(FD_ISSET(videofd, &fds))
        {
            size_t bytesused;
            int index = capture_pop(videofd, &bytesused);

            // Draw video frame
            assert(bytesused >= VIDEO_SIZE);
            graphics_draw_image(gr, buffers[index].start, VIDEO_WIDTH, VIDEO_HEIGHT, 0, 0);

            struct wptinfo wpt;

            nav_reset(wpts);
            while(nav_iter(wpts, &attd, &pos, &wpt))
            {
                // Draw waypoint label
                uint32_t x = VIDEO_WIDTH  / 2 + VIDEO_WIDTH  * wpt.x / (VIDEO_HFOV / 180.0 * M_PI);
                uint32_t y = VIDEO_HEIGHT / 2 + VIDEO_HEIGHT * wpt.y / (VIDEO_VFOV / 180.0 * M_PI);
                uint8_t color[] = FONT_COLOR;
                graphics_draw_text(gr, wpt.label, color, x, y);
            }

            // Render to screen
            if(!graphics_flush(gr)) break;
            capture_push(videofd, index);
        }
    }

    // Cleanup
    capture_stop(videofd, buffers);
    imu_close(imu, imufd);
    gps_close(gpsfd);
    nav_free(wpts);
    graphics_free(gr);

    return EXIT_SUCCESS;
}
