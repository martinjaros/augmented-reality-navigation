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

#include "gps.h"

#ifndef M_PI
#define M_PI    3.14159265358979323846
#endif

/* I/O Buffer size */
#define BUFSZ    256

/* Clean & return helper macro */
#define clean_return(fd) { close(fd); return -1; }

int gps_open(const char *devname)
{
    int fd;

    assert(devname != NULL);

    // Open device
    if((fd = open(devname, O_RDONLY | O_NOCTTY | O_NONBLOCK)) == -1) return fd;

    struct termios tty;
    bzero(&tty, sizeof(tty));
    tty.c_iflag = IGNCR;
    tty.c_oflag = 0;
    tty.c_cflag = CS8 | CREAD | CLOCAL;
    tty.c_lflag = ICANON;

    if(cfsetospeed(&tty, B9600) != 0) clean_return(fd);
    if(cfsetispeed(&tty, B9600) != 0) clean_return(fd);

    // Set attributes
    if(tcsetattr(fd, TCSANOW, &tty) != 0) clean_return(fd);

    return fd;
}

int gps_read(int fd, struct posinfo *pos)
{
    assert(fd >= 0);
    assert(pos != NULL);

    // Read NMEA sentence
    char buf[BUFSZ];
    ssize_t len = read(fd, buf, BUFSZ);
    buf[len - 1] = 0;

    // Parse GGA sentence
    double lat, lon, alt;
    if(sscanf(buf, "$GPGGA,%*d,%lf,N,%lf,E,1,%*d,%*f,%lf,M,%*f,M,,*%*s", &lat, &lon, &alt) == 3)
    {
        pos->lat = lat / 180 * M_PI;
        pos->lon = lon / 180 * M_PI;
        pos->alt = alt;
        return 1;
    }

    return 0;
}

void gps_close(int fd)
{
    close(fd);
}
