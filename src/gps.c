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
#include <time.h>

#include "debug.h"
#include "gps.h"

#define EARTH_RADIUS    6371000.0

/* Nautical mile to kilometer conversion */
#define NM2KM           1.852

/* I/O Buffer size */
#define BUFFER_SIZE     256

struct _gps
{
    int fd;
    pthread_t thread;
    pthread_mutex_t mutex;
    double lattitude, longitude, lat_offset, lon_offset;
    float altitude, speed, track, bearing, distance;
    char waypoint[32];
    struct timespec reftime;
};

static void *worker(void *arg)
{
    INFO("Thread started");
    gps_t *gps = (gps_t*)arg;

    char buf[BUFFER_SIZE];
    ssize_t len;
    while((len = read(gps->fd, buf, BUFFER_SIZE)) != -1)
    {
        if(len == 0) break;
        buf[len - 1] = 0;
        INFO("Received sentence `%s`", buf);

        double lat_deg, lon_deg, lat_min, lon_min;
        char lat_dir, lon_dir;
        float tmpf1, tmpf2;

        // Lock shared access
        pthread_mutex_lock(&gps->mutex);

        if(sscanf(buf, "$GPGGA,%*f,%2lf%lf,%c,%3lf%lf,%c,1,%*u,%*f,%f,M,%*s",
            &lat_deg, &lat_min, &lat_dir, &lon_deg, &lon_min, &lon_dir, &gps->altitude) == 7)
        {
            gps->lattitude = (lat_dir == 'N' ? 1 : -1) * (lat_deg + lat_min / 60) / 180 * M_PI;
            gps->longitude = (lon_dir == 'E' ? 1 : -1) * (lon_deg + lon_min / 60) / 180 * M_PI;

            // Set reference for interpolation
            clock_gettime(CLOCK_MONOTONIC, &gps->reftime);
            gps->lat_offset = 0;
            gps->lon_offset = 0;
        }
        else if(sscanf(buf, "$GPRMB,A,%*f,%*c,%*[^,],%32[^,],%*f,%*c,%*f,%*c,%f,%f,%*s",
            gps->waypoint, &tmpf1, &tmpf2) == 3)
        {
            gps->distance = tmpf1 * NM2KM;
            gps->bearing = tmpf2 / 180 * M_PI;
        }
        else if(sscanf(buf, "$GPRMC,%*f,A,%2lf%lf,%c,%3lf%lf,%c,%f,%f,%*s",
            &lat_deg, &lat_min, &lat_dir, &lon_deg, &lon_min, &lon_dir, &tmpf1, &tmpf2) == 8)
        {
            gps->lattitude = (lat_dir == 'N' ? 1 : -1) * (lat_deg + lat_min / 60) / 180 * M_PI;
            gps->longitude = (lon_dir == 'E' ? 1 : -1) * (lon_deg + lon_min / 60) / 180 * M_PI;
            gps->speed = tmpf1 * NM2KM;
            gps->track = tmpf2 / 180 * M_PI;

            // Set reference for interpolation
            clock_gettime(CLOCK_MONOTONIC, &gps->reftime);
            gps->lat_offset = 0;
            gps->lon_offset = 0;
        }
        else
        {
            WARN("Unknown sentence or parse error");
        }

        // Unlock shared access
        pthread_mutex_unlock(&gps->mutex);
    }

    ERROR("Broken pipe");
    return NULL;
}

gps_t *gps_init(const char *device)
{
    DEBUG("gps_init()");
    assert(device != 0);

    gps_t *gps = calloc(1, sizeof(struct _gps));
    assert(gps != 0);

    // Open device
    if((gps->fd = open(device, O_RDONLY | O_NOCTTY)) == -1)
    {
        WARN("Failed to open '%s'", device);
        free(gps);
        return NULL;
    }

    struct termios tty;
    bzero(&tty, sizeof(tty));
    tty.c_iflag = IGNCR;
    tty.c_oflag = 0;
    tty.c_cflag = CS8 | CREAD | CLOCAL;
    tty.c_lflag = ICANON;

    // Set attributes
    if(cfsetospeed(&tty, B9600) || cfsetispeed(&tty, B9600) || tcsetattr(gps->fd, TCSANOW, &tty))
    {
        WARN("Failed to set attributes");
    }

    // Start worker thread
    if(pthread_mutex_init(&gps->mutex, NULL) || pthread_create(&gps->thread, NULL, worker, gps))
    {
        WARN("Failed to create thread");
        close(gps->fd);
        free(gps);
        return NULL;
    }

    return gps;
}

void gps_get_pos(gps_t *gps, double *lat, double *lon, float *alt)
{
    DEBUG("gps_get_pos()");
    assert(gps != 0);

    pthread_mutex_lock(&gps->mutex);

    // Interpolate position
    struct timespec currtime;
    clock_gettime(CLOCK_MONOTONIC, &currtime);
    float difftime = (float)currtime.tv_sec - (float)gps->reftime.tv_sec +
                     (float)currtime.tv_nsec / 1e9 - (float)gps->reftime.tv_nsec / 1e9;
    float delta = gps->speed * 3.6 * difftime / EARTH_RADIUS;
    gps->reftime.tv_sec = currtime.tv_sec;
    gps->reftime.tv_nsec = currtime.tv_nsec;
    gps->lat_offset += cosf(gps->track) * delta;
    gps->lon_offset += sinf(gps->track) * delta / (lat ? cosf(*lat) : 1);
    if(lat) *lat = gps->lattitude + gps->lat_offset;
    if(lon) *lon = gps->longitude + gps->lon_offset;
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

void gps_free(gps_t *gps)
{
    DEBUG("gps_free()");
    assert(gps != 0);

    pthread_cancel(gps->thread);
    pthread_join(gps->thread, NULL);
    pthread_mutex_destroy(&gps->mutex);
    close(gps->fd);
    free(gps);
}
