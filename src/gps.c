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

int gps_read(int fd, struct posinfo *pos)
{
    DEBUG("gps_read()");
    assert(fd >= 0);
    assert(pos != NULL);

    // Read NMEA sentence
    char buf[BUFSZ];
    ssize_t len = read(fd, buf, BUFSZ);
    buf[len - 1] = 0;
    INFO("Received sentence `%s`", buf);

    double lat, lon, alt, spd, trk;

    // Parse GGA sentence
    if(sscanf(buf, "$GPGGA,%*d,%lf,N,%lf,E,1,%*d,%*f,%lf,M,%*s", &lat, &lon, &alt) == 3)
    {
        pos->lat = lat / 180 * M_PI;
        pos->lon = lon / 180 * M_PI;
        pos->alt = alt;
        INFO("Parsed GGA sentence latitude = %lf, longitude = %lf, altitude = %lf", pos->lat, pos->lon, pos->alt);
        return 1;
    }

    // Parse RMC sentence
    if(sscanf(buf, "$GPRMC,%*d,A,%lf,N,%lf,E,%lf,%lf,%*s", &lat, &lon, &spd, &trk) == 4)
    {
        pos->lat = lat / 180 * M_PI;
        pos->lon = lon / 180 * M_PI;
        pos->spd = spd * 1.852; // knots to km/h
        pos->trk = trk;
        INFO("Parsed RMC sentence latitude = %lf, longitude = %lf, speed = %lf, track = %lf", pos->lat, pos->lon, pos->spd, pos->trk);
        return 1;
    }

    return 0;
}

void gps_close(int fd)
{
    DEBUG("gps_close()");

    close(fd);
}
