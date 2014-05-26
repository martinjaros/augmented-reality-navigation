/*
 * GPS utilities
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

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>
#include <fcntl.h>
#include <unistd.h>
#include <termios.h>
#include <math.h>
#include <pthread.h>

#include "debug.h"
#include "gps.h"
#include "gps-util.h"

/* Earth radius in meters */
#define EARTH_RADIUS    6371000.0

/* Nautical mile to kilometer conversion */
#define NM2KM           1.852

/* km/h to m/s conversion */
#define KMH2MS          1/3.6

/* I/O Buffer size */
#define BUFFER_SIZE     256

/* NMEA 0183 maximum number of tokens */
#define MAX_TOKENS      32

struct _gps
{
    int fd;
    pthread_t thread;
    pthread_mutex_t mutex;
    double latitude, longitude;
    float altitude, speed, track, bearing, distance;
    char waypoint[32];
    struct waypoint_node *waypoint_list;
    struct dem *dem;
    const struct gps_config *config;
};

/* Splits NMEA 0183 sentence into tokens, computes checksum */
static char** split_tokens(char *s)
{
    const char hexmap[] = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'};
    static char *token_list[MAX_TOKENS];
    int checksum = 0, counter = 0;

    // Check for dolar sign
    if(*s++ != '$')
    {
        WARN("Missing dollar sign");
        return NULL;
    }
    token_list[0] = s;

    while(*s)
    {
        // Calculate checksum
        if(*s == '*')
        {
            *s = 0;
            token_list[++counter] = 0;

            if((*++s != hexmap[checksum >> 4]) || (*++s != hexmap[checksum & 0x0F]))
            {
                WARN("Bad checksum");
                return NULL;
            }

            if((*++s != '\r') || (*++s != '\n'))
            {
                WARN("Missing line termination");
                return NULL;
            }

            return token_list;
        }
        checksum ^= *s;

        // Split to tokens
        if(*s == ',')
        {
            *s = 0;
            token_list[++counter] = s + 1;
        }
        s++;
    }

    WARN("Incomplete sentence");
    return NULL;
}

static void *worker(void *arg)
{
    INFO("Thread started");
    gps_t *gps = (gps_t*)arg;

    char buf[BUFFER_SIZE];
    ssize_t len;
    while((len = read(gps->fd, buf, BUFFER_SIZE - 1)) != -1)
    {
        if(len == 0) break;
        buf[len] = 0;

        char **tokens = split_tokens(buf);
        if(tokens)
        {
            double lat_deg, lat_min, lat_dir;
            double lon_min, lon_deg, lon_dir;
            float tmpf1, tmpf2;

            if(strcmp(tokens[0], "GPGGA") == 0)
            {
                INFO("Received GGA sentence");

                // 1 - Fix time
                // 2,3 - Latitude
                if(sscanf(tokens[2], "%2lf%lf", &lat_deg, &lat_min) == 2)
                if((lat_dir = *tokens[3] == 'N' ? 1 : *tokens[3] == 'S' ? -1 : 0) != 0)
                // 4,5 - Longitude
                if(sscanf(tokens[4], "%3lf%lf", &lon_deg, &lon_min) == 2)
                if((lon_dir = *tokens[5] == 'E' ? 1 : *tokens[5] == 'W' ? -1 : 0) != 0)
                // 6 - Fix quality
                if(*tokens[6] == '1')
                // 7 - Number of satellites
                // 8 - Horizontal DOP
                // 9,10 - Altitude AMSL
                if(sscanf(tokens[9], "%f", &tmpf1) == 1)
                if(*tokens[10] == 'M')
                // 11,12 - Height of geoid above WGS84
                // 13 - time in seconds since last DGPS update
                // 14 - DGPS station ID number
                {
                    pthread_mutex_lock(&gps->mutex);
                    gps->latitude = lat_dir * (lat_deg + lat_min / 60.0) / 180.0 * M_PI;
                    gps->longitude = lon_dir * (lon_deg + lon_min / 60.0) / 180.0 * M_PI;
                    gps->altitude = tmpf1;
                    pthread_mutex_unlock(&gps->mutex);
                    continue;
                }
                goto error;
            }

            if(strcmp(tokens[0], "GPRMB") == 0)
            {
                INFO("Received GPRMB sentence");

                // 1 - Data status
                if(*tokens[1] == 'A')
                // 2,3 - Cross-track error
                // 4 - Origin waypoint name
                // 5 - Destination waypoint name
                // 6,7 - Waypoint latitude
                // 8,9 - Waypoint longitude
                // 10 - Distance
                if(sscanf(tokens[10], "%f", &tmpf1) == 1)
                // 11 - Bearing
                if(sscanf(tokens[11], "%f", &tmpf2) == 1)
                // 12 - Velocity
                // 13 - Arrival alarm
                {
                    pthread_mutex_lock(&gps->mutex);
                    strncpy(gps->waypoint, tokens[5], sizeof(gps->waypoint));
                    gps->distance = tmpf1 * NM2KM;
                    gps->bearing = tmpf2 / 180 * M_PI;
                    pthread_mutex_unlock(&gps->mutex);
                    continue;
                }
                goto error;
            }

            if(strcmp(tokens[0], "GPRMC") == 0)
            {
                INFO("Received GPRMC sentence");

                // 1 - Fix time
                // 2 - Status
                if(*tokens[2] == 'A')
                // 3,4 - Latitude
                if(sscanf(tokens[3], "%2lf%lf", &lat_deg, &lat_min) == 2)
                if((lat_dir = *tokens[4] == 'N' ? 1 : *tokens[4] == 'S' ? -1 : 0) != 0)
                // 5,6 - Longitude
                if(sscanf(tokens[5], "%3lf%lf", &lon_deg, &lon_min) == 2)
                if((lon_dir = *tokens[6] == 'E' ? 1 : *tokens[6] == 'W' ? -1 : 0) != 0)
                // 7 - Speed
                if(sscanf(tokens[7], "%f", &tmpf1) == 1)
                // 8 - Track angle
                if(sscanf(tokens[8], "%f", &tmpf2) == 1)
                // 9 - Date
                // 10 - Magnetic variation
                {
                    pthread_mutex_lock(&gps->mutex);
                    gps->latitude = lat_dir * (lat_deg + lat_min / 60.0) / 180.0 * M_PI;
                    gps->longitude = lon_dir * (lon_deg + lon_min / 60.0) / 180.0 * M_PI;
                    gps->speed = tmpf1 * NM2KM;
                    gps->track = tmpf2 / 180 * M_PI;
                    pthread_mutex_unlock(&gps->mutex);
                    continue;
                }
                goto error;
            }

            if(strcmp(tokens[0], "GPWPL") == 0)
            {
                INFO("Received GPWPL sentence");

                // 1,2 - Latitude
                if(sscanf(tokens[1], "%2lf%lf", &lat_deg, &lat_min) == 2)
                if((lat_dir = *tokens[2] == 'N' ? 1 : *tokens[2] == 'S' ? -1 : 0) != 0)
                // 3,4 - Longitude
                if(sscanf(tokens[3], "%3lf%lf", &lon_deg, &lon_min) == 2)
                if((lon_dir = *tokens[4] == 'E' ? 1 : *tokens[4] == 'W' ? -1 : 0) != 0)
                // 5 - Waypoint name
                {
                    pthread_mutex_lock(&gps->mutex);
                    struct waypoint_node *node = gps->waypoint_list;
                    while(node)
                    {
                        if(strcmp(node->name, tokens[5]) == 0)
                        {
                            // Update existing node
                            node->lat = lat_dir * (lat_deg + lat_min / 60.0) / 180.0 * M_PI;
                            node->lon = lon_dir * (lon_deg + lon_min / 60.0) / 180.0 * M_PI;
                            node->alt = gps->dem ? gps_util_dem_get_alt(gps->dem, node->lat, node->lon) : node->alt;
                            goto finish_wpl;
                        }
                        node = node->next;
                    }

                    // Create new node
                    node = malloc(sizeof(struct waypoint_node));
                    node->lat = lat_dir * (lat_deg + lat_min / 60.0) / 180.0 * M_PI;
                    node->lon = lon_dir * (lon_deg + lon_min / 60.0) / 180.0 * M_PI;
                    node->alt = gps->dem ? gps_util_dem_get_alt(gps->dem, node->lat, node->lon) : 0;
                    node->label = NULL;
                    strncpy(node->name, tokens[5], sizeof(node->name));
                    node->next = gps->waypoint_list;
                    gps->waypoint_list = node;

finish_wpl:
                    pthread_mutex_unlock(&gps->mutex);
                    continue;
                }

                goto error;
            }

            WARN("Unknown sentence: `%s`", tokens[0]);
        }
error:
        WARN("Parse error");
    }

    ERROR("Broken pipe");
    return NULL;
}

static void gps_internal_free(gps_t *gps)
{
    close(gps->fd);
    while(gps->waypoint_list)
    {
        struct waypoint_node *node = gps->waypoint_list;
        gps->waypoint_list = node->next;
        if(node->label) gps->config->delete_label(node->label);
        free(node);
    }
    if(gps->dem)
    {
        int i;
        for(i = 0; i < gps->dem->height; i++) free(gps->dem->lines[i]);
        free(gps->dem->lines);
        free(gps->dem);
    }
    free(gps);
}

gps_t *gps_init(const char *device, const struct gps_config *config)
{
    DEBUG("gps_init()");
    assert(device != 0);
    assert(config != 0);

    gps_t *gps = calloc(1, sizeof(struct _gps));
    assert(gps != 0);

    // Open device
    if((gps->fd = open(device, O_RDONLY | O_NOCTTY)) == -1)
    {
        WARN("Failed to open '%s'", device);
        free(gps);
        return NULL;
    }

    if(config->baudrate != B0)
    {
        struct termios tty;
        bzero(&tty, sizeof(tty));
        tty.c_iflag = 0;
        tty.c_oflag = 0;
        tty.c_cflag = CS8 | CREAD | CLOCAL;
        tty.c_lflag = ICANON;

        // Set attributes
        if(cfsetospeed(&tty, config->baudrate) || cfsetispeed(&tty, config->baudrate) || tcsetattr(gps->fd, TCSANOW, &tty))
        {
            WARN("Failed to set attributes");
        }
    }

    gps->config = config;
    if(config->dem_file) gps->dem = gps_util_load_demfile(config->dem_file, config->dem_left, config->dem_top, config->dem_right, config->dem_bottom, config->dem_pixel_scale);
    if(config->datafile) gps->waypoint_list = gps_util_load_datafile(config->datafile, gps->dem);

    // Start worker thread
    if(pthread_mutex_init(&gps->mutex, NULL) || pthread_create(&gps->thread, NULL, worker, gps))
    {
        WARN("Failed to create thread");
        gps_internal_free(gps);
        return NULL;
    }

    return gps;
}

void gps_get_pos(gps_t *gps, double *lat, double *lon, float *alt)
{
    DEBUG("gps_get_pos()");
    assert(gps != 0);

    pthread_mutex_lock(&gps->mutex);
    if(lat) *lat = gps->latitude;
    if(lon) *lon = gps->longitude;
    if(alt) *alt = gps->altitude;

    pthread_mutex_unlock(&gps->mutex);
}

void gps_get_track(gps_t *gps, float *speed, float *track)
{
    DEBUG("gps_get_track()");
    assert(gps != 0);

    pthread_mutex_lock(&gps->mutex);
    if(speed) *speed = gps->speed;
    if(track) *track = gps->track;
    pthread_mutex_unlock(&gps->mutex);
}

void gps_get_route(gps_t *gps, char *waypoint, float *distance, float *bearing)
{
    DEBUG("gps_get_route()");
    assert(gps != 0);

    pthread_mutex_lock(&gps->mutex);
    if(waypoint) strcpy(waypoint, gps->waypoint);
    if(distance) *distance = gps->distance;
    if(bearing) *bearing = gps->bearing;
    pthread_mutex_unlock(&gps->mutex);
}

void *gps_get_projection_label(gps_t *gps, float *hangle, float *vangle, float *dist, float att[3], void **iterator)
{
    DEBUG("gps_get_projections()");
    assert(gps != 0);
    assert(iterator != 0);

    pthread_mutex_lock(&gps->mutex);
    struct waypoint_node *node = *iterator ? (struct waypoint_node*)*iterator : gps->waypoint_list;
    if(node == NULL)
    {
        *iterator = NULL;
        return NULL;
    }

    *iterator = node->next;
    double dlat = node->lat - gps->latitude;
    double dlon = cos(gps->latitude) * (node->lon - gps->longitude);
    float dalt = node->alt - gps->altitude;
    float dist_tmp = sqrt(dlat*dlat + dlon*dlon) * EARTH_RADIUS;

    // Calculate projection angle
    float hangle_tmp = atan2(dlon, dlat) - att[2];
    float vangle_tmp = atan(dalt / dist_tmp) + att[1];
    float cosz = cos(att[0]);
    float sinz = sin(att[0]);

    // Reset to <-pi;pi> interval and rotate
    hangle_tmp = hangle_tmp < M_PI ? hangle_tmp : hangle_tmp - 2 * M_PI;
    hangle_tmp = hangle_tmp > -M_PI ? hangle_tmp : hangle_tmp + 2 * M_PI;
    vangle_tmp = vangle_tmp < M_PI ? vangle_tmp : vangle_tmp - 2 * M_PI;
    vangle_tmp = vangle_tmp > -M_PI ? vangle_tmp : vangle_tmp + 2 * M_PI;
    if(hangle) *hangle = hangle_tmp * cosz - vangle_tmp * sinz;
    if(vangle) *vangle = hangle_tmp * sinz - vangle_tmp * cosz;
    if(dist) *dist = dist_tmp;
    if(!node->label) node->label = gps->config->create_label(node->name, gps->config->userdata);
    pthread_mutex_unlock(&gps->mutex);

    return node->label;
}

void gps_inertial_update(gps_t *gps, float dvx, float dvy, float dvz, float dt)
{
    DEBUG("gps_get_projections()");
    assert(gps != 0);

    INFO("Inertial update dvx = %f, dvy = %f, dvz = %f, dt = %f", dvx, dvy, dvz, dt);
    dvx *= dt / KMH2MS;
    dvy *= dt / KMH2MS;
    dvx += cosf(gps->track) * gps->speed;
    dvy += sinf(gps->track) * gps->speed;
    gps->track = atan2f(dvy, dvx);
    gps->speed = sqrt(dvx * dvx + dvy * dvy);

    pthread_mutex_lock(&gps->mutex);
    float delta = gps->speed * KMH2MS * dt / EARTH_RADIUS;
    double tmp = gps->latitude += cosf(gps->track) * delta;
    gps->longitude += sinf(gps->track) * delta / cosf(tmp);
    gps->altitude += dvz * dt;
    pthread_mutex_unlock(&gps->mutex);
}

void gps_free(gps_t *gps)
{
    DEBUG("gps_free()");
    assert(gps != 0);

    pthread_cancel(gps->thread);
    pthread_join(gps->thread, NULL);
    pthread_mutex_destroy(&gps->mutex);
    gps_internal_free(gps);
}
