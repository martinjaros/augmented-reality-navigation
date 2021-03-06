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
#include <termios.h>

#include "debug.h"
#include "application.h"

#define BUFFER_SIZE     128

#ifdef X11BUILD
#include <xcb/xcb.h>
static unsigned long window_create(uint32_t width, uint32_t height)
{
    xcb_connection_t *conn = xcb_connect(NULL, NULL);
    if(xcb_connection_has_error(conn)) { ERROR("Cannot connect to display"); return 0; }

    xcb_screen_t *screen = xcb_setup_roots_iterator(xcb_get_setup(conn)).data;
    xcb_window_t window = xcb_generate_id(conn);

    // Create window
    xcb_create_window(conn, XCB_COPY_FROM_PARENT, window, screen->root, 0, 0, width, height, 0, XCB_WINDOW_CLASS_INPUT_OUTPUT, screen->root_visual, 0, NULL);
    xcb_map_window(conn, window);
    xcb_flush(conn);

    return window;
}
#else /* X11BUILD */
#define window_create(width, height)  0
#endif

int main(int argc, char *argv[])
{
    DEBUG("main()");

    // Base configuration
    static struct config cfg =
    {
        .app_landmark_vis_dist = 5000,

        .video_device = "/dev/video0",
        .video_width = 800,
        .window_width = 800,
        .video_height = 600,
        .window_height = 600,
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
        .imu_conf =
        {
            .gyro_offset = { 0, 0, 0 },
            .gyro_weight = .8,
            .gyro_scale = 0.00053264847315724, // 1000 deg/s
            .acc_scale = 0.000244140625, // 8g
        },

        .gps_device = "/dev/null"
    };

    if(argc > 1)
    {
        INFO("Using config file `%s`", argv[1]);

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
                int baudrate = 0;
                if(sscanf(str, "app_landmarks_file = %ms", &cfg.gps_conf.datafile) != 1)
                if(sscanf(str, "app_landmark_vis_dist = %f", &cfg.app_landmark_vis_dist) != 1)
                if(sscanf(str, "window_width = %u", &cfg.window_width) != 1)
                if(sscanf(str, "window_height = %u", &cfg.window_height) != 1)
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
                if(sscanf(str, "imu_gyro_offset_x = %f", &cfg.imu_conf.gyro_offset[0]) != 1)
                if(sscanf(str, "imu_gyro_offset_y = %f", &cfg.imu_conf.gyro_offset[1]) != 1)
                if(sscanf(str, "imu_gyro_offset_z = %f", &cfg.imu_conf.gyro_offset[2]) != 1)
                if(sscanf(str, "imu_gyro_weight = %f", &cfg.imu_conf.gyro_weight) != 1)
                if(sscanf(str, "imu_gyro_scale = %f", &cfg.imu_conf.gyro_scale) != 1)
                if(sscanf(str, "gps_device = %ms", &cfg.gps_device) != 1)
                if(sscanf(str, "gps_dem_file = %ms", &cfg.gps_conf.dem_file) != 1)
                if(sscanf(str, "gps_dem_left = %lf", &cfg.gps_conf.dem_left) != 1)
                if(sscanf(str, "gps_dem_top = %lf", &cfg.gps_conf.dem_top) != 1)
                if(sscanf(str, "gps_dem_right = %lf", &cfg.gps_conf.dem_right) != 1)
                if(sscanf(str, "gps_dem_bottom = %lf", &cfg.gps_conf.dem_bottom) != 1)
                if(sscanf(str, "gps_dem_pixel_scale = %f", &cfg.gps_conf.dem_pixel_scale) != 1)
                if(sscanf(str, "gps_baudrate = %d", &baudrate) != 1)
                {
                    WARN("Unknown parameter or parse error");
                    continue;
                }

                switch(baudrate)
                {
                    case 4800:
                        cfg.gps_conf.baudrate = B4800;
                        break;

                    case 9600:
                        cfg.gps_conf.baudrate = B9600;
                        break;

                    case 19200:
                        cfg.gps_conf.baudrate = B19200;
                        break;

                    case 38400:
                        cfg.gps_conf.baudrate = B38400;
                        break;

                    case 57600:
                        cfg.gps_conf.baudrate = B57600;
                        break;

                    case 115200:
                        cfg.gps_conf.baudrate = B115200;
                        break;

                    default:
                        cfg.gps_conf.baudrate = B0;
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

            fclose(f);
        }
    }

    // Create window
    cfg.app_window_id = window_create(cfg.window_width, cfg.window_height);

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
