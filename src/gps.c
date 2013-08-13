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
#include <string.h>
#include <assert.h>
#include <fcntl.h>
#include <unistd.h>
#include <termios.h>
#include <math.h>

#include "debug.h"
#include "gps.h"

#ifndef M_PI
#define M_PI    3.14159265358979323846
#endif

/* Nautical mile to kilometer conversion */
#define NM2KM   1.852

/* I/O Buffer size */
#define BUFSZ    256

int gps_open(const char *devname)
{
    DEBUG("gps_open()");
    int fd;

    assert(devname != NULL);

    // Open device
    if((fd = open(devname, O_RDONLY | O_NOCTTY | O_NONBLOCK)) == -1)
    {
        WARN("Failed to open '%s'", devname);
        return fd;
    }

    struct termios tty;
    bzero(&tty, sizeof(tty));
    tty.c_iflag = IGNCR;
    tty.c_oflag = 0;
    tty.c_cflag = CS8 | CREAD | CLOCAL;
    tty.c_lflag = ICANON;

    if(cfsetospeed(&tty, B9600) != 0)
    {
        WARN("Failed to set output baud rate");
        close(fd);
        return -1;
    }
    if(cfsetispeed(&tty, B9600) != 0)
    {
        WARN("Failed to set input baud rate");
        close(fd);
        return -1;
    }

    // Set attributes
    if(tcsetattr(fd, TCSANOW, &tty) != 0)
    {
        WARN("Failed to set attributes");
        close(fd);
        return -1;
    }

    return fd;
}

int gps_read(int fd, struct gpsdata *data)
{
    DEBUG("gps_read()");
    assert(fd >= 0);
    assert(data != NULL);

    // Read NMEA sentence
    char buf[BUFSZ];
    ssize_t len = read(fd, buf, BUFSZ);
    buf[len - 1] = 0;
    INFO("Received sentence `%s`", buf);

    float hr, min, sec, lat_deg, lat_min, lon_deg, lon_min, alt, spd, trk, dst, xte;
    char lat_dir, lon_dir, *orig = NULL, *dest = NULL, xte_dir;

    // Parse GGA sentence
    if(sscanf(buf, "$GPGGA,%2f%2f%f,%2f%f,%c,%3f%f,%c,1,%*d,%*f,%f,M,%*s",
              &hr, &min, &sec, &lat_deg, &lat_min, &lat_dir, &lon_deg, &lon_min, &lon_dir, &alt) == 10)
    {
        data->time = hr * 3600.0 + min * 60.0 + sec;
        data->lat = (lat_dir == 'E' ? 1 : -1) * (lat_deg + (lat_min / 60.0)) / 180 * M_PI;
        data->lon = (lon_dir == 'E' ? 1 : -1) * (lon_deg + (lon_min / 60.0)) / 180 * M_PI;
        data->alt = alt;
        INFO("Parsed GGA sentence time = %lf, lat = %lf, lon = %lf, alt = %lf",
             data->time, data->lat, data->lon, data->alt);
        return 1;
    }

    // Parse RMB sentence
    if(sscanf(buf, "$GPRMB,A,%f,%c,%m[^,],%m[^,],%2f%f,%c,%3f%f,%c,%f,%f,%f,%*s",
              &xte, &xte_dir, &orig, &dest, &lat_deg, &lat_min, &lat_dir, &lon_deg, &lon_min, &lon_dir, &dst, &trk, &spd) == 13)
    {
        memcpy(data->orig_name, orig, sizeof(data->orig_name));
        memcpy(data->dest_name, dest, sizeof(data->dest_name));
        free(orig);
        free(dest);

        data->xte = (xte_dir == 'R' ? 1 : -1 ) * xte * NM2KM;
        data->dest_lat = (lat_dir == 'E' ? 1 : -1) * (lat_deg + (lat_min / 60.0)) / 180 * M_PI;
        data->dest_lon = (lon_dir == 'E' ? 1 : -1) * (lon_deg + (lon_min / 60.0)) / 180 * M_PI;
        data->dest_range = dst * NM2KM;
        data->dest_bearing = trk;
        data->dest_velocity = spd * NM2KM;
        INFO("Parsed RMB sentence xte = %lf, orig = %s, dest = %s, lat = %lf, lon = %lf, range = %lf, bearing = %lf, velocity = %lf",
             data->xte, data->orig_name, data->dest_name, data->dest_lat, data->dest_lon, data->dest_range, data->dest_bearing, data->dest_velocity);
        return 1;
    }
    if(orig) free(orig);
    if(dest) free(dest);

    // Parse RMC sentence
    if(sscanf(buf, "$GPRMC,%2f%2f%f,A,%2f%f,%c,%3f%f,%c,%f,%f,%*s",
              &hr, &min, &sec, &lat_deg, &lat_min, &lat_dir, &lon_deg, &lon_min, &lon_dir, &spd, &trk) == 11)
    {
        data->time = hr * 3600.0 + min * 60.0 + sec;
        data->lat = (lat_dir == 'E' ? 1 : -1) * (lat_deg + (lat_min / 60.0)) / 180 * M_PI;
        data->lon = (lon_dir == 'E' ? 1 : -1) * (lon_deg + (lon_min / 60.0)) / 180 * M_PI;
        data->spd = spd * NM2KM;
        data->trk = trk;
        INFO("Parsed RMC sentence time = %lf, lat = %lf, lon = %lf, speed = %lf, track = %lf",
             data->time, data->lat, data->lon, data->spd, data->trk);
        return 1;
    }

    return 0;
}

void gps_close(int fd)
{
    DEBUG("gps_close()");

    close(fd);
}
