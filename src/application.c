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
#include <fcntl.h>
#include <unistd.h>
#include <math.h>

#include "debug.h"
#include "application.h"
#include "graphics.h"
#include "video.h"
#include "gps.h"
#include "imu.h"

#define EARTH_RADIUS  6371000.0
#define BUFFER_SIZE   64

struct landmark_node
{
    double lat, lon;
    float alt;
    drawable_t *label;
    struct landmark_node *next;
};

struct _application
{
    imu_t *imu;
    gps_t *gps;
    video_t *video;
    graphics_t *graphics;
    atlas_t *atlas1, *atlas2;
    drawable_t *image;
    drawable_t *label_alt, *label_spd, *label_trk;
    drawable_t *label_waypoint, *label_range, *label_bearing;
    struct landmark_node *landmark;

    uint32_t video_width, video_height;
    float video_hfov, video_vfov;
};

application_t *application_init(struct config *cfg)
{
    DEBUG("application_init()");
    assert(cfg != 0);

    application_t *app = calloc(1, sizeof(struct _application));
    assert(app != 0);

    // Initialize graphics
    if(!(app->graphics = graphics_init(cfg->app_window_id)))
    {
        ERROR("Cannot initialize graphics");
        goto error;
    }

    // Create atlases
    if(!(app->atlas1 = graphics_atlas_create(cfg->graphics_font_file, cfg->graphics_font_size_1)) ||
       !(app->atlas2 = graphics_atlas_create(cfg->graphics_font_file, cfg->graphics_font_size_2)))
    {
        ERROR("Cannot create atlas");
        goto error;
    }

    // Initialize GPS
    if(!(app->gps = gps_init(cfg->gps_device)))
    {
        ERROR("Cannot initialize GPS");
        goto error;
    }

    // Initialize IMU
    if(!(app->imu = imu_init(cfg->imu_device, cfg->imu_gyro_scale, cfg->imu_gyro_offset, cfg->imu_mag_declination, cfg->imu_mag_inclination, cfg->imu_mag_weight, cfg->imu_acc_weight)))
    {
        ERROR("Cannot initialize IMU");
        goto error;
    }

    if(strcmp(cfg->video_format, "RGB4"))
    {
        ERROR("Video format not supported");
        goto error;
    }

    // Open video
    if(!(app->video = video_open(cfg->video_device, cfg->video_width, cfg->video_height, cfg->video_format, cfg->video_interlace)))
    {
        ERROR("Cannot open video device");
        goto error;
    }

    // Create drawables
    app->image = graphics_image_create(app->graphics, cfg->video_width, cfg->video_height, ANCHOR_LEFT_TOP);
    app->label_alt = graphics_label_create(app->graphics, app->atlas1, ANCHOR_LEFT_TOP);
    app->label_spd = graphics_label_create(app->graphics,app->atlas1, ANCHOR_LEFT_TOP);
    app->label_trk = graphics_label_create(app->graphics,app->atlas1, ANCHOR_LEFT_TOP);
    app->label_waypoint = graphics_label_create(app->graphics,app->atlas1, ANCHOR_RIGHT_TOP);
    app->label_range = graphics_label_create(app->graphics,app->atlas1, ANCHOR_RIGHT_TOP);
    app->label_bearing = graphics_label_create(app->graphics,app->atlas1, ANCHOR_RIGHT_TOP);
    if(!app->image || !app->label_alt || !app->label_spd || !app->label_trk ||
       !app->label_waypoint || !app->label_range || !app->label_bearing)
    {
        ERROR("Cannot create widgets");
        goto error;
    }

    graphics_label_set_color(app->label_alt, cfg->graphics_font_color_1);
    graphics_label_set_color(app->label_spd, cfg->graphics_font_color_1);
    graphics_label_set_color(app->label_trk, cfg->graphics_font_color_1);
    graphics_label_set_color(app->label_waypoint, cfg->graphics_font_color_1);
    graphics_label_set_color(app->label_range, cfg->graphics_font_color_1);
    graphics_label_set_color(app->label_bearing, cfg->graphics_font_color_1);

    // Read landmarks
    if(cfg->app_landmarks_file)
    {
        FILE *f = fopen(cfg->app_landmarks_file, "r");
        if(!f)
        {
            WARN("Failed to open `%s`", cfg->app_landmarks_file);
        }
        else
        {
            struct landmark_node *node = NULL, *node_prev = NULL;

            char buf[BUFFER_SIZE];
            while(fgets(buf, BUFFER_SIZE, f))
            {
                char *str = buf;

                // Skip empty lines and '#' comments
                while(*str == ' ') str++;
                if((*str == '\n') || (*str == '#')) continue;
                str[strlen(str) - 1] = 0;
                INFO("Parsing landmark line `%s`", str);

                node = malloc(sizeof(struct landmark_node));

                // Parse line
                char name[32];
                if(sscanf(str, "%lf, %lf, %f, %32[^\n]", &node->lat, &node->lon, &node->alt, name) != 4)
                {
                    WARN("Parse error");
                    free(node);
                    continue;
                }

                node->label = graphics_label_create(app->graphics, app->atlas2, ANCHOR_CENTER_BOTTOM);
                graphics_label_set_color(node->label, cfg->graphics_font_color_2);
                graphics_label_set_text(node->label, name);
                node->next = NULL;
                if(node_prev)
                {
                    node_prev = node_prev->next = node;
                }
                else
                {
                    app->landmark = node_prev = node;
                }
            }
        }
    }

    // Copy arguments
    app->video_width = cfg->video_width;
    app->video_height = cfg->video_height;
    app->video_hfov = cfg->video_hfov;
    app->video_vfov = cfg->video_vfov;

    return app;

error:
    if(app->video) video_close(app->video);
    if(app->gps) gps_free(app->gps);
    if(app->imu) imu_free(app->imu);
    if(app->image) graphics_drawable_free(app->image);
    if(app->label_alt) graphics_drawable_free(app->label_alt);
    if(app->label_spd) graphics_drawable_free(app->label_spd);
    if(app->label_trk) graphics_drawable_free(app->label_trk);
    if(app->label_waypoint) graphics_drawable_free(app->label_waypoint);
    if(app->label_bearing) graphics_drawable_free(app->label_bearing);
    if(app->label_range) graphics_drawable_free(app->label_range);
    if(app->atlas1) graphics_atlas_free(app->atlas1);
    if(app->atlas2) graphics_atlas_free(app->atlas2);
    if(app->graphics) graphics_free(app->graphics);
    while(app->landmark)
    {
        struct landmark_node *node = app->landmark;
        app->landmark = node->next;
        free(node);
    }

    free(app);
    return NULL;
}

void application_mainloop(application_t *app)
{
    DEBUG("application_mainloop()");
    assert(app != 0);

    void *data;
    size_t length;
    double lat, lon;
    float roll, pitch, yaw, alt, speed, track, bearing, range;
    char waypoint[32], str[128];

    while(1)
    {
        // Process video
        if(!video_read(app->video, &data, &length))
        {
            ERROR("Cannot read from video device");
            break;
        }
        assert(length == app->video_width * app->video_height * 4);
        graphics_image_set_bitmap(app->image, data);
        graphics_draw(app->graphics, app->image, 0, 0, 1, 0);

        // Draw landmarks
        imu_get_attitude(app->imu, &roll, &pitch, &yaw);
        gps_get_pos(app->gps, &lat, &lon, &alt);
        struct landmark_node *node;
        for(node = app->landmark; node; node = node->next)
        {
            double dlat = node->lat - lat;
            double dlon = cos(lat) * (node->lon - lon);
            float dalt = node->alt - alt;
            float dist = sqrt(dlat*dlat + dlon*dlon) * EARTH_RADIUS;
            float hangle_tmp = atan2(dlon, dlat) - yaw;
            float vangle_tmp = atan(dalt / dist) - pitch;
            float cosz = cos(roll);
            float sinz = sin(roll);
            float hangle = hangle_tmp * cosz - vangle_tmp * sinz;
            float vangle = hangle_tmp * sinz - vangle_tmp * cosz;

            if((hangle < app->video_hfov / -2.0) || (hangle > app->video_hfov / 2.0) || (vangle < app->video_vfov / -2.0) || (vangle > app->video_vfov / 2.0)) continue;
            INFO("Projecting landmark hangle = %f, vangle = %f, distance = %f", hangle, vangle, dist / 1000.0);
            uint32_t x = (float)app->video_width  / 2 + (float)app->video_width  * hangle / app->video_hfov;
            uint32_t y = (float)app->video_height / 2 + (float)app->video_height * vangle / app->video_vfov;
            graphics_draw(app->graphics, node->label, x, y, 1, 0);
        }

        // Draw track info
        gps_get_track(app->gps, &speed, &track);

        snprintf(str, sizeof(str), "Altitude %d m", (int)(alt + 0.5));
        graphics_label_set_text(app->label_alt, str);
        graphics_draw(app->graphics, app->label_alt, 10, 10, 1, 0);

        snprintf(str, sizeof(str), "Speed %d km/h", (int)(speed + 0.5));
        graphics_label_set_text(app->label_spd, str);
        graphics_draw(app->graphics, app->label_spd, 10, 40, 1, 0);

        snprintf(str, sizeof(str), "Track %d deg", (int)(track + 0.5));
        graphics_label_set_text(app->label_trk, str);
        graphics_draw(app->graphics, app->label_trk, 10, 70, 1, 0);

        // Draw route info
        gps_get_route(app->gps, waypoint, &range, &bearing);

        snprintf(str, sizeof(str), "Destination `%s`", waypoint);
        graphics_label_set_text(app->label_waypoint, str);
        graphics_draw(app->graphics, app->label_waypoint, app->video_width - 10, 10, 1, 0);

        snprintf(str, sizeof(str), "Bearing %d\xB0", (int)(bearing + 0.5));
        graphics_label_set_text(app->label_bearing, str);
        graphics_draw(app->graphics, app->label_bearing, app->video_width - 10, 40, 1, 0);

        snprintf(str, sizeof(str), "Range %d km", (int)(range + 0.5));
        graphics_label_set_text(app->label_range, str);
        graphics_draw(app->graphics, app->label_range, app->video_width - 10, 70, 1, 0);

        // Render to screen
        if(!graphics_flush(app->graphics, NULL))
        {
            ERROR("Cannot draw");
            break;
        }
    }
}

void application_free(application_t *app)
{
    DEBUG("application_free()");
    assert(app != 0);

    video_close(app->video);
    gps_free(app->gps);
    imu_free(app->imu);
    graphics_drawable_free(app->image);
    graphics_drawable_free(app->label_alt);
    graphics_drawable_free(app->label_spd);
    graphics_drawable_free(app->label_trk);
    graphics_drawable_free(app->label_waypoint);
    graphics_drawable_free(app->label_bearing);
    graphics_drawable_free(app->label_range);
    graphics_atlas_free(app->atlas1);
    graphics_atlas_free(app->atlas2);
    graphics_free(app->graphics);
    while(app->landmark)
    {
        struct landmark_node *node = app->landmark;
        app->landmark = node->next;
        free(node);
    }
    free(app);
}
