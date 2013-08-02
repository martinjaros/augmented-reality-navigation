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

#include "application.h"
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

struct _application
{
    // File descriptors
    int nfds;
    int gpsfd, imufd, videofd;

    // Graphics state
    graphics_t *graphics;
    atlas_t *atlas;
    drawable_t *image, *label_alt, *label_spd, *label_trk;

    // Video state
    struct buffer *buffers;

    // Navigation state
    wpt_iter *wpts;
    imu_t *imu;
    struct posinfo pos;
    struct attdinfo attd;
};

application_t *application_init(struct config *cfg)
{
    assert(cfg != NULL);

    // Allocate structure
    application_t *app = calloc(1, sizeof(struct _application));
    assert(app != NULL);

    // Initialize graphics
    if((app->graphics = graphics_init()) == NULL)
    {
        fprintf(stderr, "Cannot initialize graphics\n");
        return NULL;
    }

    // Create font atlas
    if((app->atlas = graphics_atlas_create(cfg->fontname, FONT_SIZE)) == NULL)
    {
        fprintf(stderr, "Cannot load %s\n", cfg->fontname);
        return NULL;
    }

    // Load waypoints
    uint8_t color[] = FONT_COLOR;
    if((app->wpts = nav_load(cfg->wptf, app->graphics, app->atlas, color)) == NULL)
    {
        fprintf(stderr, "Cannot load %s\n", cfg->wptf);
        return NULL;
    }

    // Open GPS
    if((app->gpsfd = gps_open(cfg->gpsdev)) < 0)
    {
        fprintf(stderr, "Cannot open %s\n", cfg->gpsdev);
        return NULL;
    }

    // Open IMU
    app->imu = imu_open(cfg->imudev);
    if((app->imufd = imu_timer_create(app->imu)) < 0)
    {
        fprintf(stderr, "Cannot open %s\n", cfg->imudev);
        return NULL;
    }

    // Start video capture
    if((app->videofd = capture_start(cfg->videodev, VIDEO_WIDTH, VIDEO_HEIGHT, VIDEO_FORMAT, VIDEO_INTERLACE, &app->buffers)) < 0)
    {
        fprintf(stderr, "Cannot open %s\n", cfg->videodev);
        return NULL;
    }

    // Create video image and global labels
    app->image = graphics_image_create(app->graphics, VIDEO_WIDTH, VIDEO_HEIGHT);
    app->label_alt = graphics_label_create(app->graphics);
    app->label_spd = graphics_label_create(app->graphics);
    app->label_trk = graphics_label_create(app->graphics);

    // Calculate highest file descriptor + 1
    app->nfds = MAX(app->gpsfd, app->nfds);
    app->nfds = MAX(app->imufd, app->nfds);
    app->nfds = MAX(app->videofd, app->nfds);
    app->nfds++;

    return app;
}

void application_mainloop(application_t *app)
{
    assert(app != NULL);

    while(1)
    {
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(app->gpsfd, &fds);
        FD_SET(app->imufd, &fds);
        FD_SET(app->videofd, &fds);
        select(app->nfds, &fds, NULL, NULL, NULL);

        // Process GPS
        if(FD_ISSET(app->gpsfd, &fds))
        {
            if(gps_read(app->gpsfd, &app->pos))
            {
                char str[128];

                snprintf(str, sizeof(str), "Altitude %d m", (int)(app->pos.alt + 0.5));
                graphics_label_set_text(app->graphics, app->label_alt, app->atlas, str);

                snprintf(str, sizeof(str), "Speed %d km/h", (int)(app->pos.spd + 0.5));
                graphics_label_set_text(app->graphics, app->label_spd, app->atlas, str);

                snprintf(str, sizeof(str), "Track %d deg", (int)(app->pos.trk + 0.5));
                graphics_label_set_text(app->graphics, app->label_trk, app->atlas, str);
            }
        }

        // Process IMU
        if(FD_ISSET(app->imufd, &fds))
        {
            imu_read(app->imu, &app->attd);
        }

        // Process video
        if(FD_ISSET(app->videofd, &fds))
        {
            size_t bytesused;
            int index = capture_pop(app->videofd, &bytesused);

            // Draw video frame
            assert(bytesused >= VIDEO_SIZE);
            graphics_image_set_bitmap(app->graphics, app->image, app->buffers[index].start);
            graphics_draw(app->graphics, app->image, 0, 0);

            struct wptinfo wpt;

            nav_reset(app->wpts);
            while(nav_iter(app->wpts, &app->attd, &app->pos, &wpt))
            {
                // Draw waypoint label
                uint32_t x = VIDEO_WIDTH  / 2 + VIDEO_WIDTH  * wpt.x / (VIDEO_HFOV / 180.0 * M_PI);
                uint32_t y = VIDEO_HEIGHT / 2 + VIDEO_HEIGHT * wpt.y / (VIDEO_VFOV / 180.0 * M_PI);
                if((x > 0) && (x < VIDEO_WIDTH) && (y > 0) && (y < VIDEO_HEIGHT))
                {
                    graphics_draw(app->graphics, wpt.label, x, y);
                }
            }

            // Draw global labels
            graphics_draw(app->graphics, app->label_alt, 10, 10);
            graphics_draw(app->graphics, app->label_spd, 10, 10 + FONT_SIZE * 3 / 2);
            graphics_draw(app->graphics, app->label_trk, 10, 10 + FONT_SIZE * 3);

            // Render to screen
            if(!graphics_flush(app->graphics, NULL)) break;
            capture_push(app->videofd, index);
        }
    }
}

void application_free(application_t *app)
{
    capture_stop(app->videofd, app->buffers);
    imu_close(app->imu, app->imufd);
    gps_close(app->gpsfd);
    nav_free(app->wpts);
    graphics_free(app->graphics);
    free(app);
}
