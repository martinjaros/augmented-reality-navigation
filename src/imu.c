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
#include <pthread.h>
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
    int devfd, timerfd, is_alive;
    double matrix[9];
    pthread_t thread;
    pthread_mutex_t mutex;
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

/* Select helper function */
static void timerfd_select(int fd)
{
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(fd, &fds);

    select(fd + 1, &fds, NULL, NULL, NULL);
}

/* IMU thread worker */
static void *imu_worker(void *arg)
{
    assert(arg != NULL);

    int i;
    struct driver_data data;
    double matrix[9], calib[3] = { 0, 0, 0};
    imu_t *imu = (imu_t*)arg;

    // Initialize driver
    if(!driver_init(imu->devfd))
    {
        WARN("Cannot initialize driver");
        imu->is_alive = 0;
        return NULL;
    }

    struct itimerspec timer;
    timer.it_interval.tv_sec = 0;
    timer.it_interval.tv_nsec = TIMER_ITER_MS * 1000000;
    timer.it_value.tv_sec = timer.it_interval.tv_sec;
    timer.it_value.tv_nsec = timer.it_interval.tv_nsec;

    // Set timer
    if((timerfd_settime(imu->timerfd, 0, &timer, NULL)) == -1)
    {
        WARN("Failed to set timer");
        imu->is_alive = 0;
        return NULL;
    }

    // Skip initial measurements
    for(i = 0; i < GYRO_CALIB_SKIP; i++)
    {
        timerfd_select(imu->timerfd);
        if(!driver_read(imu->devfd, &data)) goto error;
    }

    // Calibrate gyroscope
    for(i = 0; i < GYRO_CALIB_STEPS; i++)
    {
        timerfd_select(imu->timerfd);
        if(!driver_read(imu->devfd, &data)) goto error;
        calib[0] += data.gyro[0];
        calib[1] += data.gyro[1];
        calib[2] += data.gyro[2];
        continue;
    }

    // Finalize calibration
    calib[0] /= GYRO_CALIB_STEPS;
    calib[1] /= GYRO_CALIB_STEPS;
    calib[2] /= GYRO_CALIB_STEPS;

    // Initialize rotation matrix
    matrix[0] = data.mag[0];
    matrix[1] = data.mag[1];
    matrix[2] = data.mag[2];
    vector_normalize(&matrix[0]);

    matrix[6] = data.acc[0];
    matrix[7] = data.acc[1];
    matrix[8] = data.acc[2];
    vector_normalize(&matrix[6]);

    matrix_orthogonalize(matrix);
    INFO("IMU calibration finished");

    while(1)
    {
        timerfd_select(imu->timerfd);
        if(!driver_read(imu->devfd, &data)) goto error;
        INFO("Compass readings [%lf, %lf, %lf]", data.mag[0], data.mag[1], data.mag[2]);
        INFO("Gyroscope readings [%lf, %lf, %lf]", data.gyro[0], data.gyro[1], data.gyro[2]);
        INFO("Accelerometer readings [%lf, %lf, %lf]", data.acc[0], data.acc[1], data.acc[2]);

        // Calculate weighted average
        vector_normalize(data.mag);
        vector_normalize(data.acc);
        matrix[0] = (1 - WEIGHT_MAG) * matrix[0] + WEIGHT_MAG * data.mag[0];
        matrix[1] = (1 - WEIGHT_MAG) * matrix[1] + WEIGHT_MAG * data.mag[1];
        matrix[2] = (1 - WEIGHT_MAG) * matrix[2] + WEIGHT_MAG * data.mag[2];
        matrix[6] = (1 - WEIGHT_ACC) * matrix[6] + WEIGHT_ACC * data.acc[0];
        matrix[7] = (1 - WEIGHT_ACC) * matrix[7] + WEIGHT_ACC * data.acc[1];
        matrix[8] = (1 - WEIGHT_ACC) * matrix[8] + WEIGHT_ACC * data.acc[2];
        vector_normalize(&matrix[0]);
        vector_normalize(&matrix[6]);
        matrix_orthogonalize(matrix);

        // Rotate matrix
        double rx = (data.gyro[0] - calib[0]) * TIMER_ITER_MS;
        double ry = (data.gyro[1] - calib[1]) * TIMER_ITER_MS;
        double rz = (data.gyro[2] - calib[2]) * TIMER_ITER_MS;
        matrix_rotate(matrix, rx, ry, rz);

        // Save matrix
        pthread_mutex_lock(&imu->mutex);
        for(i = 0; i < 9; i++) imu->matrix[i] = matrix[i];
        pthread_mutex_unlock(&imu->mutex);
    }

error:
    WARN("Failed to read from driver");
    imu->is_alive = 0;
    return NULL;
}

imu_t *imu_open(const char *devname)
{
    DEBUG("imu_open()");
    assert(devname != NULL);

    // Initialize state structure
    imu_t *imu = calloc(1, sizeof(struct _imu));
    if(imu == NULL)
    {
        WARN("Cannot allocate memory");
        return NULL;
    }

    // Open device
    if((imu->devfd = open(devname, O_RDWR)) == -1)
    {
        WARN("Failed to open `%s`", devname);
        free(imu);
        return NULL;
    }

    // Create timer
    if((imu->timerfd = timerfd_create(CLOCK_REALTIME, O_NONBLOCK)) == -1)
    {
        WARN("Failed to create timer");
        close(imu->devfd);
        free(imu);
        return NULL;
    }

    // Start worker thread
    if(pthread_mutex_init(&imu->mutex, NULL) || pthread_create(&imu->thread, NULL, imu_worker, imu))
    {
        WARN("Failed to create thread");
        close(imu->timerfd);
        close(imu->devfd);
        free(imu);
        return NULL;
    }
    imu->is_alive = 1;

    return imu;
}

int imu_read(imu_t *imu, struct imudata *data)
{
    DEBUG("imu_read()");
    assert(imu != NULL);
    assert(data != NULL);

    if(!imu->is_alive)
    {
        WARN("Thread is dead");
        return 0;
    }

    // Get absolute angles
    pthread_mutex_lock(&imu->mutex);
    data->roll = atan2(imu->matrix[5], imu->matrix[8]);
    data->pitch = asin(imu->matrix[2]);
    data->yaw = atan2(imu->matrix[1], imu->matrix[0]);
    pthread_mutex_unlock(&imu->mutex);

    INFO("Attitude roll = %lf, pitch = %lf, yaw = %lf", data->roll, data->pitch, data->yaw);
    return 1;
}

void imu_close(imu_t *imu)
{
    DEBUG("imu_close()");
    assert(imu != NULL);

    pthread_cancel(imu->thread);
    pthread_detach(imu->thread);
    pthread_mutex_destroy(&imu->mutex);
    close(imu->timerfd);
    close(imu->devfd);
    free(imu);
}
