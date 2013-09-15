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

/* Nautical mile to kilometer conversion */
#define NM2KM           1.852

/* I/O Buffer size */
#define BUFFER_SIZE     256

struct _gps
{
    int fd;
    pthread_t thread;
    pthread_mutex_t mutex;

    uint16_t hr, min, lat_deg, lon_deg, sat_num;
    float sec, lat_min, lon_min, alt, spd, trk, brg, dst, xte;
    char lat_dir, lon_dir, xte_dir, orig[32], dest[32];
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

        pthread_mutex_lock(&gps->mutex);
        if(sscanf(buf, "$GPGGA,%2hu%2hu%f,%2hu%f,%c,%3hu%f,%c,1,%hu,%*f,%f,M,%*s",
                  &gps->hr, &gps->min, &gps->sec, &gps->lat_deg, &gps->lat_min, &gps->lat_dir, &gps->lon_deg, &gps->lon_min, &gps->lon_dir, &gps->sat_num, &gps->alt) != 11)
        if(sscanf(buf, "$GPRMB,A,%f,%c,%32[^,],%32[^,],%2hu%f,%c,%3hu%f,%c,%f,%f,%f,%*s",
                  &gps->xte, &gps->xte_dir, gps->orig, gps->dest, &gps->lat_deg, &gps->lat_min, &gps->lat_dir, &gps->lon_deg, &gps->lon_min, &gps->lon_dir, &gps->dst, &gps->brg, &gps->spd) != 13)
        if(sscanf(buf, "$GPRMC,%2hu%2hu%f,A,%2hu%f,%c,%3hu%f,%c,%f,%f,%*s",
                  &gps->hr, &gps->min, &gps->sec, &gps->lat_deg, &gps->lat_min, &gps->lat_dir, &gps->lon_deg, &gps->lon_min, &gps->lon_dir, &gps->spd, &gps->trk) != 11)
        {
            WARN("Unknown sentence or parse error");
        }
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

void gps_get_time(gps_t *gps, double *time)
{
    DEBUG("gps_get_time()");
    assert(gps != 0);

    pthread_mutex_lock(&gps->mutex);
    if(time) *time = (float)gps->hr * 3600.0 + (float)gps->min * 60.0 + gps->sec;
    pthread_mutex_unlock(&gps->mutex);
}

void gps_get_pos(gps_t *gps, double *lat, double *lon, float *alt)
{
    DEBUG("gps_get_pos()");
    assert(gps != 0);

    pthread_mutex_lock(&gps->mutex);
    if(lat) *lat = (gps->lat_dir == 'N' ? 1 : -1) * ((float)gps->lat_deg + (gps->lat_min / 60.0)) / 180 * M_PI;
    if(lon) *lon = (gps->lon_dir == 'E' ? 1 : -1) * ((float)gps->lon_deg + (gps->lon_min / 60.0)) / 180 * M_PI;
    if(alt) *alt = gps->alt;
    pthread_mutex_unlock(&gps->mutex);
}

void gps_get_track(gps_t *gps, float *speed, float *track)
{
    DEBUG("gps_get_track()");
    assert(gps != 0);

    pthread_mutex_lock(&gps->mutex);
    if(speed) *speed = gps->spd * NM2KM;
    if(track) *track = gps->trk;
    pthread_mutex_unlock(&gps->mutex);
}

void gps_get_route(gps_t *gps, char *waypoint, float *range, float *bearing)
{
    DEBUG("gps_get_route()");
    assert(gps != 0);

    pthread_mutex_lock(&gps->mutex);
    if(waypoint) strcpy(waypoint, gps->dest);
    if(range) *range = gps->dst * NM2KM;
    if(bearing) *bearing = gps->brg;
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
