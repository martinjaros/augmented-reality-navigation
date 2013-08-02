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

#include "application.h"

/* Prints command line usage */
static void usage()
{
    printf("Augmented reality navigation\n"
           "usage: arnav [--help] [--video=<device>] [--gps=<device>] [--imu=<device>] [--wpts=<filename>] [--font=<fontname>]\n"
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
    struct config cfg =
    {
        .videodev = VIDEODEV,
        .gpsdev = GPSDEV,
        .imudev = IMUDEV,
        .wptf = WPTF,
        .fontname = FONTNAME
    };

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
            case 1: cfg.videodev = strdup(optarg); break;
            case 2: cfg.gpsdev = strdup(optarg); break;
            case 3: cfg.imudev = strdup(optarg); break;
            case 4: cfg.wptf = strdup(optarg); break;
            case 5: cfg.fontname = strdup(optarg); break;
        }
    }

    // Start application
    application_t *app = application_init(&cfg);
    if(app)
    {
        application_mainloop(app);
        application_free(app);
        return EXIT_SUCCESS;
    }

    return EXIT_FAILURE;
}
