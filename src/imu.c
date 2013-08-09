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
#include <sys/timerfd.h>

#include "debug.h"
#include "imu.h"
#include "imu-driver.h"

#ifndef M_PI
#define M_PI    3.14159265358979323846
#endif

/* Timer period */
#define TIMER_ITER_MS   10

/* Number of calibration steps */
#define GYRO_CALIB_SKIP     2
#define GYRO_CALIB_STEPS    30

/* Base weights for averaging */
#define WEIGHT_ACC  0.5
#define WEIGHT_MAG  0.5

/* IMU state structure */
struct _imu
{
    double calib[3];
    double matrix[9];
    int fd, steps;
};

/* 3-axis orientation vector normalization helper function */
static void vector_normalize(double v[3])
{
    double len = sqrt(v[0]*v[0] + v[1]*v[1] + v[2]*v[2]);
    v[0] /= len;
    v[1] /= len;
    v[2] /= len;
}

/* 3x3 rotation matrix orthogonalization helper function */
static void matrix_orthogonalize(double m[9])
{
    // Half of the dot product (orthogonal error coefficient)
    double dot_2 = (m[0]*m[6] + m[1]*m[7] + m[2]*m[8]) / 2;

    // Orthogonalized compass vector
    double mag[3] =
    {
        m[0] - dot_2 * m[6],
        m[1] - dot_2 * m[7],
        m[2] - dot_2 * m[8]
    };
    vector_normalize(mag);

    // Orthogonalized accelerometer vector
    double acc[3] =
    {
        m[6] - dot_2 * m[0],
        m[7] - dot_2 * m[1],
        m[8] - dot_2 * m[2]
    };
    vector_normalize(acc);

    // Compose 3x3 matrix
    m[0] = mag[0];
    m[1] = mag[1];
    m[2] = mag[2];
    m[3] = acc[1] * mag[2] - acc[2] * mag[1];
    m[4] = acc[2] * mag[0] - acc[0] * mag[2];
    m[5] = acc[0] * mag[1] - acc[1] * mag[0];
    m[6] = acc[0];
    m[7] = acc[1];
    m[8] = acc[2];
}

/* 3x3 matrix rotation helper function */
static void matrix_rotate(double m[9], double rx, double ry, double rz)
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
    DEBUG("imu_open()");
    imu_t *imu;

    assert(devname != NULL);

    // Open device
    int fd;
    if((fd = open(devname, O_RDWR | O_NONBLOCK)) == -1)
    {
        WARN("Failed to open `%s`", devname);
        return NULL;
    }

    // Initialize state structure
    imu = malloc(sizeof(struct _imu));
    if(imu == NULL)
    {
        WARN("Cannot allocate memory");
        return NULL;
    }
    imu->fd = fd;
    imu->steps = -GYRO_CALIB_SKIP;
    imu->calib[0] = 0;
    imu->calib[1] = 0;
    imu->calib[2] = 0;

    // Initialize driver
    if(!driver_init(fd))
    {
        WARN("Cannot initialize IMU driver");
        close(fd);
        free(imu);
        return NULL;
    }

    return imu;
}

int imu_timer_create()
{
    DEBUG("imu_timer_create()");
    int timerfd;

    // Create timer
    if((timerfd = timerfd_create(CLOCK_REALTIME, O_NONBLOCK)) == -1)
    {
        WARN("Failed to create timer");
        return timerfd;
    }

    struct itimerspec timer;
    timer.it_interval.tv_sec = 0;
    timer.it_interval.tv_nsec = TIMER_ITER_MS * 1000000;
    timer.it_value.tv_sec = timer.it_interval.tv_sec;
    timer.it_value.tv_nsec = timer.it_interval.tv_nsec;

    // Set timer
    if((timerfd_settime(timerfd, 0, &timer, NULL)) == -1)
    {
        WARN("Failed to set timer");
        close(timerfd);
        return -1;
    }

    return timerfd;
}

int imu_read(imu_t *imu, struct attdinfo *attd)
{
    DEBUG("imu_read()");
    assert(imu != NULL);
    assert(attd != NULL);

    // Read data from driver
    struct driver_data data;
    driver_read(imu->fd, &data);

    // Skip initial measurements
    if(imu->steps++ < 0) return 0;

    // Calibration steps
    if(imu->steps++ < GYRO_CALIB_STEPS)
    {
        imu->calib[0] += data.gyro[0];
        imu->calib[1] += data.gyro[1];
        imu->calib[2] += data.gyro[2];
        return 0;
    }
    else if(imu->steps++ == GYRO_CALIB_STEPS)
    {
        // Finalize calibration
        imu->calib[0] /= GYRO_CALIB_STEPS;
        imu->calib[1] /= GYRO_CALIB_STEPS;
        imu->calib[2] /= GYRO_CALIB_STEPS;

        // Initialize rotation matrix
        imu->matrix[0] = data.mag[0];
        imu->matrix[1] = data.mag[1];
        imu->matrix[2] = data.mag[2];
        vector_normalize(&imu->matrix[0]);

        imu->matrix[6] = data.acc[0];
        imu->matrix[7] = data.acc[1];
        imu->matrix[8] = data.acc[2];
        vector_normalize(&imu->matrix[6]);

        matrix_orthogonalize(imu->matrix);
        INFO("IMU calibration finished");
    }
    else
    {
        INFO("Compass readings [%lf, %lf, %lf]", data.mag[0], data.mag[1], data.mag[2]);
        INFO("Gyroscope readings [%lf, %lf, %lf]", data.gyro[0], data.gyro[1], data.gyro[2]);
        INFO("Accelerometer readings [%lf, %lf, %lf]", data.acc[0], data.acc[1], data.acc[2]);

        // Calculate weighted average
        vector_normalize(data.mag);
        vector_normalize(data.acc);
        imu->matrix[0] = (1 - WEIGHT_MAG) * imu->matrix[0] + WEIGHT_MAG * data.mag[0];
        imu->matrix[1] = (1 - WEIGHT_MAG) * imu->matrix[1] + WEIGHT_MAG * data.mag[1];
        imu->matrix[2] = (1 - WEIGHT_MAG) * imu->matrix[2] + WEIGHT_MAG * data.mag[2];
        imu->matrix[6] = (1 - WEIGHT_ACC) * imu->matrix[6] + WEIGHT_ACC * data.acc[0];
        imu->matrix[7] = (1 - WEIGHT_ACC) * imu->matrix[7] + WEIGHT_ACC * data.acc[1];
        imu->matrix[8] = (1 - WEIGHT_ACC) * imu->matrix[8] + WEIGHT_ACC * data.acc[2];
        vector_normalize(&imu->matrix[0]);
        vector_normalize(&imu->matrix[6]);
        matrix_orthogonalize(imu->matrix);
    }

    // Rotate matrix
    double rx = (data.gyro[0] - imu->calib[0]) * TIMER_ITER_MS;
    double ry = (data.gyro[1] - imu->calib[1]) * TIMER_ITER_MS;
    double rz = (data.gyro[2] - imu->calib[2]) * TIMER_ITER_MS;
    matrix_rotate(imu->matrix, rx, ry, rz);

    // Get absolute angles
    attd->roll = atan2(imu->matrix[5], imu->matrix[8]);
    attd->pitch = asin(imu->matrix[2]);
    attd->yaw = atan2(imu->matrix[1], imu->matrix[0]);

    INFO("Attitude roll = %lf, pitch = %lf, yaw = %lf", attd->roll, attd->pitch, attd->yaw);

    return 1;
}

void imu_close(imu_t *imu, int fd)
{
    DEBUG("imu_close()");
    assert(imu != NULL);

    if(fd > -1) close(fd);
    close(imu->fd);
    free(imu);
}
