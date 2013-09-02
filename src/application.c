/*
 * ARNav application
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
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <math.h>

#include "debug.h"
#include "application.h"
#include "graphics.h"
#include "capture.h"
#include "gps.h"
#include "imu.h"
#include "nav.h"

#ifndef M_PI
#define M_PI    3.14159265358979323846
#endif

struct _application
{
    // File descriptors
    int nfds;
    int gpsfd, videofd;

    // Graphics state
    graphics_t *graphics;
    atlas_t *atlas;
    drawable_t *image;
    drawable_t *label_alt, *label_spd, *label_trk;
    drawable_t *label_dest_name, *label_dest_range, *label_dest_bearing;

    // Video state
    struct buffer *buffers;

    // Navigation state
    wpt_iter *wpts;
    imu_t *imu;
    struct gpsdata gpsdata;
    struct imudata imudata;
};

/* Resource cleanup helper function */
static void cleanup(application_t *app)
{
    if(app->videofd > 0) capture_stop(app->videofd, app->buffers);
    if(app->gpsfd > 0) gps_close(app->gpsfd);
    if(app->imu) imu_close(app->imu);
    if(app->wpts) nav_free(app->wpts);
    if(app->image) graphics_drawable_free(app->image);
    if(app->label_alt) graphics_drawable_free(app->label_alt);
    if(app->label_spd) graphics_drawable_free(app->label_spd);
    if(app->label_trk) graphics_drawable_free(app->label_trk);
    if(app->label_dest_name) graphics_drawable_free(app->label_dest_name);
    if(app->label_dest_bearing) graphics_drawable_free(app->label_dest_bearing);
    if(app->label_dest_range) graphics_drawable_free(app->label_dest_range);
    if(app->atlas) graphics_atlas_free(app->atlas);
    if(app->graphics) graphics_free(app->graphics);
    free(app);
}

application_t *application_init(struct config *cfg)
{
    DEBUG("application_init()");
    assert(cfg != NULL);

    // Allocate structure
    application_t *app = calloc(1, sizeof(struct _application));
    if(app == NULL)
    {
        ERROR("Cannot allocate memory");
        return NULL;
    }

    // Initialize graphics
    if((app->graphics = graphics_init(graphics_window_create(VIDEO_WIDTH, VIDEO_HEIGHT))) == NULL)
    {
        ERROR("Cannot initialize graphics");
        cleanup(app);
        return NULL;
    }

    // Create font atlas
    if((app->atlas = graphics_atlas_create(cfg->fontname, FONT_SIZE)) == NULL)
    {
        ERROR("Cannot create font atlas");
        cleanup(app);
        return NULL;
    }

    // Load waypoints
    uint8_t color[] = FONT_COLOR;
    if((app->wpts = nav_load(cfg->wptf, app->graphics, app->atlas, color)) == NULL)
    {
        ERROR("Cannot load waypoints");
        cleanup(app);
        return NULL;
    }

    // Open GPS
    if((app->gpsfd = gps_open(cfg->gpsdev)) < 0)
    {
        ERROR("Cannot initialize GPS device");
        cleanup(app);
        return NULL;
    }

    // Load IMU calibration
    struct imucalib calib;
    if(!imu_calib_load(&calib, cfg->calibname))
    {
        ERROR("Cannot load IMU calibration");
        cleanup(app);
        return NULL;
    }

    // Open IMU
    if((app->imu = imu_open(cfg->imudev, &calib)) == NULL)
    {
        ERROR("Cannot initialize IMU device");
        cleanup(app);
        return NULL;
    }

    // Start video capture
    if((app->videofd = capture_start(cfg->videodev, VIDEO_WIDTH, VIDEO_HEIGHT, VIDEO_FORMAT, VIDEO_INTERLACE, &app->buffers)) < 0)
    {
        ERROR("Cannot start video capture");
        cleanup(app);
        return NULL;
    }

    // Create video image and global labels
    app->image = graphics_image_create(app->graphics, VIDEO_WIDTH, VIDEO_HEIGHT);
    app->label_alt = graphics_label_create(app->graphics, app->atlas);
    app->label_spd = graphics_label_create(app->graphics,app->atlas);
    app->label_trk = graphics_label_create(app->graphics,app->atlas);
    app->label_dest_name = graphics_label_create(app->graphics,app->atlas);
    app->label_dest_range = graphics_label_create(app->graphics,app->atlas);
    app->label_dest_bearing = graphics_label_create(app->graphics,app->atlas);
    if(!app->image || !app->label_alt || !app->label_spd || !app->label_trk ||
       !app->label_dest_name || !app->label_dest_range || !app->label_dest_bearing)
    {
        ERROR("Cannot create graphics widgets");
        cleanup(app);
        return NULL;
    }

    // Calculate highest file descriptor + 1
    app->nfds = app->gpsfd > app->videofd ? app->gpsfd + 1 : app->videofd + 1;

    return app;
}

void application_mainloop(application_t *app)
{
    DEBUG("application_mainloop()");
    assert(app != NULL);

    while(1)
    {
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(app->gpsfd, &fds);
        FD_SET(app->videofd, &fds);

        DEBUG("select()");
        select(app->nfds, &fds, NULL, NULL, NULL);

        // Process GPS
        if(FD_ISSET(app->gpsfd, &fds))
        {
            if(gps_read(app->gpsfd, &app->gpsdata))
            {
                char str[128];

                snprintf(str, sizeof(str), "Altitude %d m", (int)(app->gpsdata.alt + 0.5));
                graphics_label_set_text(app->label_alt, ANCHOR_LEFT_TOP, str);

                snprintf(str, sizeof(str), "Speed %d km/h", (int)(app->gpsdata.spd + 0.5));
                graphics_label_set_text(app->label_spd, ANCHOR_LEFT_TOP, str);

                snprintf(str, sizeof(str), "Track %d deg", (int)(app->gpsdata.trk + 0.5));
                graphics_label_set_text(app->label_trk, ANCHOR_LEFT_TOP, str);

                if(app->gpsdata.dest_name[0])
                {
                    snprintf(str, sizeof(str), "Destination %s", app->gpsdata.dest_name);
                    graphics_label_set_text(app->label_dest_name, ANCHOR_RIGHT_TOP, str);

                    snprintf(str, sizeof(str), "Bearing %d°", (int)(app->gpsdata.dest_bearing + 0.5));
                    graphics_label_set_text(app->label_dest_bearing, ANCHOR_RIGHT_TOP, str);

                    snprintf(str, sizeof(str), "Range %d km", (int)(app->gpsdata.dest_range + 0.5));
                    graphics_label_set_text(app->label_dest_range, ANCHOR_RIGHT_TOP, str);
                }
                else
                {
                    graphics_label_set_text(app->label_dest_name, ANCHOR_RIGHT_TOP, "");
                    graphics_label_set_text(app->label_dest_bearing, ANCHOR_RIGHT_TOP, "");
                    graphics_label_set_text(app->label_dest_range, ANCHOR_RIGHT_TOP, "");
                }
            }
        }

        // Process video
        if(FD_ISSET(app->videofd, &fds))
        {
            size_t bytesused;
            int index = capture_pop(app->videofd, &bytesused);

            // Read from IMU
            if(!imu_read(app->imu, &app->imudata))
            {
                ERROR("Cannot read from IMU device");
                break;
            }

            // Draw video frame
            assert(bytesused >= VIDEO_SIZE);
            graphics_image_set_bitmap(app->image, app->buffers[index].start);
            graphics_draw(app->graphics, app->image, 0, 0);

            struct wptdata wpt;

            nav_reset(app->wpts);
            while(nav_iter(app->wpts, &app->imudata, &app->gpsdata, &wpt))
            {
                // Draw waypoint label
                uint32_t x = VIDEO_WIDTH  / 2 + VIDEO_WIDTH  * wpt.x / (VIDEO_HFOV / 180.0 * M_PI);
                uint32_t y = VIDEO_HEIGHT / 2 + VIDEO_HEIGHT * wpt.y / (VIDEO_VFOV / 180.0 * M_PI);
                if((x > 0) && (x < VIDEO_WIDTH) && (y > 0) && (y < VIDEO_HEIGHT))
                {
                    graphics_draw(app->graphics, wpt.label, x, y);
                }
                else INFO("Waypoint clipped before drawing");
            }

            // Draw global labels
            graphics_draw(app->graphics, app->label_alt, 10, 10);
            graphics_draw(app->graphics, app->label_spd, 10, 10 + FONT_SIZE * 3 / 2);
            graphics_draw(app->graphics, app->label_trk, 10, 10 + FONT_SIZE * 3);
            if(app->gpsdata.dest_name[0])
            {
                graphics_draw(app->graphics, app->label_dest_name, VIDEO_WIDTH - 10, 10);
                graphics_draw(app->graphics, app->label_dest_bearing, VIDEO_WIDTH - 10, 10 + FONT_SIZE * 3 / 2);
                graphics_draw(app->graphics, app->label_dest_range, VIDEO_WIDTH - 10, 10 + FONT_SIZE * 3);
            }

            // Render to screen
            if(!graphics_flush(app->graphics, NULL))
            {
                ERROR("Cannot draw");
                break;
            }
            capture_push(app->videofd, index);
        }
    }
}

void application_free(application_t *app)
{
    DEBUG("application_free()");
    assert(app != NULL);

    cleanup(app);
}
