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

#ifdef X11BUILD
#include <X11/Xlib.h>
#endif /* X11BUILD */

#include "debug.h"
#include "application.h"

#define BUFFER_SIZE     128

static unsigned long window_create(uint32_t width, uint32_t height)
{
#ifdef X11BUILD

    // Open display
    Display *display = XOpenDisplay(NULL);
    if(!display)
    {
        WARN("Failed to open X display");
        return 0;
    }

    // Create window
    unsigned long color = BlackPixel(display, 0);
    Window root = RootWindow(display, 0);
    Window window = XCreateSimpleWindow(display, root, 0, 0, width, height, 0, color, color);
    XMapWindow(display, window);
    XFlush(display);

    INFO("Created window 0x%x", (uint32_t)window);
    return window;

#else /* X11BUILD */
    return 0;
#endif
}

int main(int argc, char *argv[])
{
    DEBUG("main()");

    int i;
    for(i = 0; i < argc; i++) INFO("Argument %d = %s", i, argv[i]);

    // Base configuration
    static struct config cfg =
    {
        .video_device = "/dev/video0",
        .video_width = 800,
        .video_height = 600,
        .video_format = "RGB4",
        .video_interlace = 0,
        .video_hfov = 1.0471, // 60 deg
        .video_vfov = 1.0471,

        .graphics_font_file = "/usr/share/fonts/truetype/freefont/FreeSans.ttf",
        .graphics_font_color_1 = { 0, 0, 0, 255 },
        .graphics_font_color_2 = { 0, 0, 0, 255 },
        .graphics_font_size_1 = 20,
        .graphics_font_size_2 = 12,

        .imu_device = "/dev/null",
        .imu_gyro_scale = 1,
        .imu_mag_weight = 0.2,
        .imu_acc_weight = 0.2,

        .gps_device = "/dev/null"
    };

    if(argc == 2)
    {
        // Read configuration
        FILE *f = fopen(argv[1], "r");
        if(!f)
        {
            WARN("Failed to open `%s`", argv[1]);
        }
        else
        {
            char buf[BUFFER_SIZE];
            while(fgets(buf, BUFFER_SIZE, f))
            {
                char *str = buf;

                // Skip empty lines and '#' comments
                while(*str == ' ') str++;
                if((*str == '\n') || (*str == '#')) continue;
                str[strlen(str) - 1] = 0;
                INFO("Parsing config line `%s`", str);

                // Parse line
                char *interlace = NULL;
                if(sscanf(str, "app_landmarks_file = %ms", &cfg.app_landmarks_file) != 1)
                if(sscanf(str, "video_device = %ms", &cfg.video_device) != 1)
                if(sscanf(str, "video_width = %u", &cfg.video_width) != 1)
                if(sscanf(str, "video_height = %u", &cfg.video_height) != 1)
                if(sscanf(str, "video_format = %4c", cfg.video_format) != 1)
                if(sscanf(str, "video_interlace = %ms", &interlace) != 1)
                if(sscanf(str, "video_hfov = %f", &cfg.video_hfov) != 1)
                if(sscanf(str, "video_vfov = %f", &cfg.video_vfov) != 1)
                if(sscanf(str, "graphics_font_file = %ms", &cfg.graphics_font_file) != 1)
                if(sscanf(str, "graphics_font_color_1 = %x", (uint32_t*)cfg.graphics_font_color_1) != 1)
                if(sscanf(str, "graphics_font_color_2 = %x", (uint32_t*)cfg.graphics_font_color_2) != 1)
                if(sscanf(str, "graphics_font_size_1 = %hhu", &cfg.graphics_font_size_1) != 1)
                if(sscanf(str, "graphics_font_size_2 = %hhu", &cfg.graphics_font_size_2) != 1)
                if(sscanf(str, "imu_device = %ms", &cfg.imu_device) != 1)
                if(sscanf(str, "imu_gyro_scale = %f", &cfg.imu_gyro_scale) != 1)
                if(sscanf(str, "imu_gyro_offset_x = %f", &cfg.imu_gyro_offset[0]) != 1)
                if(sscanf(str, "imu_gyro_offset_y = %f", &cfg.imu_gyro_offset[1]) != 1)
                if(sscanf(str, "imu_gyro_offset_z = %f", &cfg.imu_gyro_offset[2]) != 1)
                if(sscanf(str, "imu_mag_declination = %f", &cfg.imu_mag_declination) != 1)
                if(sscanf(str, "imu_mag_inclination = %f", &cfg.imu_mag_inclination) != 1)
                if(sscanf(str, "imu_mag_weight = %f", &cfg.imu_mag_weight) != 1)
                if(sscanf(str, "imu_acc_weight = %f", &cfg.imu_acc_weight) != 1)
                if(sscanf(str, "gps_device = %ms", &cfg.gps_device) != 1)
                {
                    WARN("Unknown parameter or parse error");
                    continue;
                }

                if(interlace)
                {
                    if(!strcmp(interlace, "true"))
                    {
                        cfg.video_interlace = 1;
                    }
                    else if(!strcmp(interlace, "false"))
                    {
                        cfg.video_interlace = 0;
                    }
                    else
                    {
                        WARN("Parse error");
                    }
                    free(interlace);
                }
            }
        }
    }

    // Create window
    cfg.app_window_id = window_create(cfg.video_width, cfg.video_height);

    // Start application
    application_t *app = application_init(&cfg);
    if(app)
    {
        application_mainloop(app);
        application_free(app);
        return EXIT_SUCCESS;
    }
    else
    {
        return EXIT_FAILURE;
    }
}
