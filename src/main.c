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

#include "debug.h"
#include "test.h"
#include "application.h"
#include "imu-utils.h"

/* Prints command line usage */
static void usage()
{
    printf("Augmented reality navigation\n"
           "usage: arnav [--help] [--test[=<test>]] [--video=<device>] [--gps=<device>] [--imu=<device>] [--wpts=<filename>] [--font=<fontname>] [--calib=<calibname>] [--update-calib]\n"
           "   --help               Shows help\n"
           "   --test[=<test>]      Executes unit tests\n"
           "   --video <device>     Sets video device to use (\"/dev/video*\")\n"
           "   --gps <device>       Sets GPS device to use (\"/dev/tty*\")\n"
           "   --imu <device>       Sets IMU device to use (\"/dev/i2c-*\")\n"
           "   --wpts <file>        Sets waypoint file to use\n"
           "   --font <file>        Sets TrueType font file to use\n"
           "   --calib <file>       Sets calibration file to use\n"
           "   --update-calib       Re-calibrates device in interactive mode\n"
           "\n");
}

int main(int argc, char *argv[])
{
    DEBUG("main()");

    struct config cfg =
    {
        .videodev = VIDEODEV,
        .gpsdev = GPSDEV,
        .imudev = IMUDEV,
        .wptf = WPTF,
        .fontname = FONTNAME,
        .calibname = CALIBNAME
    };

    static struct option options[] = {
        { "help",           no_argument,       NULL, 0 },
        { "test",           optional_argument, NULL, 1 },
        { "video",          required_argument, NULL, 2 },
        { "gps",            required_argument, NULL, 3 },
        { "imu",            required_argument, NULL, 4 },
        { "wpts",           required_argument, NULL, 5 },
        { "font",           required_argument, NULL, 6 },
        { "calib",          required_argument, NULL, 7 },
        { "update-calib",   no_argument,       NULL, 8 },
        { 0, 0, 0, 0}
    };

    // Parse options
    char *test = NULL;
    int index, calib = 0;
    while((index = getopt_long(argc, argv, "", options, NULL)) != -1)
    {
        switch(index)
        {
            case 0:  usage(); return EXIT_SUCCESS;
            case 1:  test = optarg ? strdup(optarg) : (char*)-1; break;
            case 2:  cfg.videodev = strdup(optarg); break;
            case 3:  cfg.gpsdev = strdup(optarg); break;
            case 4:  cfg.imudev = strdup(optarg); break;
            case 5:  cfg.wptf = strdup(optarg); break;
            case 6:  cfg.fontname = strdup(optarg); break;
            case 7:  cfg.calibname = strdup(optarg); break;
            case 8:  calib = 1; break;
            default: usage(); return EXIT_FAILURE;
        }
    }

    if(calib)
    {
        // Run calibration
        struct imucalib calib;
        if(!imu_calib_generate(&calib, cfg.imudev) || !imu_calib_save(&calib, cfg.calibname))
        {
            ERROR("Calibration failed");
        }
    }

    if(test)
    {
        // Execute tests
        int counter = 0;

        if((test == (char*)-1) || (strcmp(test, "capture") == 0))
        {
            INFO("Testing capture, videodev='%s'", cfg.videodev);
            if(!test_capture(cfg.videodev, VIDEO_WIDTH, VIDEO_HEIGHT, 100)) ERROR("Capture test failed"); else counter++;
        }

        if((test == (char*)-1) || (strcmp(test, "gps") == 0))
        {
            INFO("Testing GPS, gpsdev='%s'", cfg.gpsdev);
            if(!test_gps(cfg.gpsdev, 20)) ERROR("GPS test failed"); else counter++;
        }

        if((test == (char*)-1) || (strcmp(test, "imu") == 0))
        {
            INFO("Testing IMU, imudev='%s'", cfg.imudev);
            if(!test_imu(cfg.imudev, cfg.calibname, 50)) ERROR("IMU test failed"); else counter++;
        }

        if((test == (char*)-1) || (strcmp(test, "graphics") == 0))
        {
            INFO("Testing graphics, fontname='%s'", cfg.fontname);
            if(!test_graphics(cfg.fontname, VIDEO_WIDTH, VIDEO_HEIGHT, 100)) ERROR("Graphics test failed"); else counter++;
        }

        INFO("%d tests completed sucessfully", counter);
        return EXIT_SUCCESS;
    }
    else
    {
        INFO("Initializing videodev='%s', gpsdev='%s', imudev='%s', wptf='%s', fontname='%s', calibname='%s'",
             cfg.videodev, cfg.gpsdev, cfg.imudev, cfg.wptf, cfg.fontname, cfg.calibname);

        // Start application
        application_t *app = application_init(&cfg);
        if(app)
        {
            application_mainloop(app);
            application_free(app);
            return EXIT_SUCCESS;
        }
        else ERROR("Initialization failed");
    }

    return EXIT_FAILURE;
}
