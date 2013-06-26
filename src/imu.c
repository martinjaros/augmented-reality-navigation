/*
 * Inertial measurement unit
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
#include <math.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/timerfd.h>
#include <linux/i2c.h>
#include <linux/i2c-dev.h>

#include "imu.h"
#include "itg-3200.h"

#ifndef M_PI
#define M_PI    3.14159265358979323846
#endif

/* Timer period */
#define TIMER_ITER_MS   5

/* Device definitions */
#define GYRO_ADDR       ITG3200_I2C_ADDR0
#define GYRO_REG        ITG3200_REG_GYRO_XOUT_H
#define GYRO_CONF       { ITG3200_REG_SMPLRT_DIV, 0, (3 << 3) | 1, 0 }

#define GYRO_SCALE          (2000.0 / 180.0 * M_PI) / 32767.0 / 1000.0 * TIMER_ITER_MS
#define GYRO_CALIB_SKIP     10
#define GYRO_CALIB_STEPS    100

/* IMU state structure */
struct _imu
{
    double calib[3];
    double matrix[9];
    int fd, steps;
};

/* Matrix rotation helper function */
static void rotate_matrix(double m[9], double rx, double ry, double rz)
{
    double r[9] =
    {
            m[0] + rz*m[3] - ry*m[6],
            m[1] + rz*m[4] - ry*m[7],
            m[2] + rz*m[5] - ry*m[8],

            (rx*ry-rz)*m[0] + (rx*ry*rz+1)*m[3] + rx*m[6],
            (rx*ry-rz)*m[1] + (rx*ry*rz+1)*m[4] + rx*m[7],
            (rx*ry-rz)*m[2] + (rx*ry*rz+1)*m[5] + rx*m[8],

            (rx*rz+ry)*m[0] + (ry*rz-rx)*m[3] + m[6],
            (rx*rz+ry)*m[1] + (ry*rz-rx)*m[4] + m[7],
            (rx*rz+ry)*m[2] + (ry*rz-rx)*m[5] + m[8]
    };

    int i;
    for(i = 0; i < 9; i++) m[i] = r[i];
}

imu_t *imu_open(const char *devname)
{
    imu_t *imu;

    assert(devname != NULL);

    // Open device
    int fd;
    if((fd = open(devname, O_RDWR | O_NONBLOCK)) == -1) return NULL;

    // Allocate state structure
    imu = malloc(sizeof(struct _imu));
    imu->fd = fd;
    imu->steps = -GYRO_CALIB_SKIP;
    imu->calib[0] = 0;
    imu->calib[1] = 0;
    imu->calib[2] = 0;
    imu->matrix[0] = 1; imu->matrix[1] = 0; imu->matrix[2] = 0;
    imu->matrix[3] = 0; imu->matrix[4] = 1; imu->matrix[5] = 0;
    imu->matrix[6] = 0; imu->matrix[7] = 0; imu->matrix[8] = 1;

    unsigned char gyro_conf[] = GYRO_CONF;

    struct i2c_msg msg[] =
    {
        {
            .addr = GYRO_ADDR,
            .flags = 0,
            .len = sizeof(gyro_conf),
            .buf = gyro_conf
        }
    };

    struct i2c_rdwr_ioctl_data msgset;
    msgset.msgs = msg;
    msgset.nmsgs = sizeof(msg) / sizeof(struct i2c_msg);

    // Write configuration
    if(ioctl(fd, I2C_RDWR, &msgset) == -1)
    {
        close(fd);
        free(imu);
        return NULL;
    }

    return imu;
}

int imu_timer_create()
{
    int timerfd;

    // Create timer
    if((timerfd = timerfd_create(CLOCK_REALTIME, O_NONBLOCK)) == -1) return timerfd;

    struct itimerspec timer;
    timer.it_interval.tv_sec = 0;
    timer.it_interval.tv_nsec = TIMER_ITER_MS * 1000000;
    timer.it_value.tv_sec = timer.it_interval.tv_sec;
    timer.it_value.tv_nsec = timer.it_interval.tv_nsec;

    // Set timer
    if((timerfd_settime(timerfd, 0, &timer, NULL)) == -1)
    {
        close(timerfd);
        return -1;
    }

    return timerfd;
}

int imu_read(imu_t *imu, struct attdinfo *attd)
{
    assert(imu != NULL);
    assert(attd != NULL);

    unsigned char gyro_buf[6] = { GYRO_REG };

    struct i2c_msg msg[] =
    {
        {
            .addr = GYRO_ADDR,
            .flags = 0,
            .len = sizeof(gyro_buf),
            .buf = gyro_buf
        }
    };

    struct i2c_rdwr_ioctl_data msgset;
    msgset.msgs = msg;
    msgset.nmsgs = sizeof(msg) / sizeof(struct i2c_msg);

    // Read data
    if(ioctl(imu->fd, I2C_RDWR, &msgset) == -1) return 0;

    // Skip initial measurements
    if(imu->steps++ < 0) return 0;

    if(imu->steps++ < GYRO_CALIB_STEPS)
    {
        // Calibrate
        imu->calib[0] += (short)(gyro_buf[0] << 8 | gyro_buf[1]);
        imu->calib[1] += (short)(gyro_buf[2] << 8 | gyro_buf[3]);
        imu->calib[2] += (short)(gyro_buf[4] << 8 | gyro_buf[5]);
        return 0;
    }

    // Get values
    double rx = ((short)(gyro_buf[0] << 8 | gyro_buf[1]) - imu->calib[0]) * GYRO_SCALE;
    double ry = ((short)(gyro_buf[2] << 8 | gyro_buf[3]) - imu->calib[1]) * GYRO_SCALE;
    double rz = ((short)(gyro_buf[4] << 8 | gyro_buf[5]) - imu->calib[2]) * GYRO_SCALE;

    // Rotate matrix
    rotate_matrix(imu->matrix, rx, ry, rz);
    attd->roll = atan2(imu->matrix[5], imu->matrix[8]);
    attd->pitch = asin(imu->matrix[2]);
    attd->yaw = atan2(imu->matrix[1], imu->matrix[0]);

    return 1;
}

void imu_close(imu_t *imu, int fd)
{
    assert(imu != NULL);

    close(fd);
    close(imu->fd);
    free(imu);
}
