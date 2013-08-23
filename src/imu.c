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

/* I/O buffer size */
#define BUFFER_SIZE     128

/* IMU state structure */
struct _imu
{
    int devfd, timerfd, is_alive;
    double matrix[9];
    struct imucalib calib;
    pthread_t thread;
    pthread_mutex_t mutex;
};

/* Normalizes 3D vector */
static void vector_normalize(double v[3])
{
    double len = sqrt(v[0]*v[0] + v[1]*v[1] + v[2]*v[2]);
    v[0] /= len;
    v[1] /= len;
    v[2] /= len;
}

/* Rotates vector around Y and Z axis */
static void vector_rotate_yz(double v[3], double ry, double rz)
{
    // Y rotation
    double sinr = sin(ry);
    double cosr = cos(ry);
    double tmp[3] =
    {
        cosr*v[0] + sinr*v[2],
        v[1],
        cosr*v[2] - sinr*v[0]
    };

    // Z rotation
    sinr = sin(rz);
    cosr = cos(rz);
    v[0] = cosr*tmp[0] - sinr*tmp[1];
    v[1] = sinr*tmp[0] + cosr*tmp[1];
    v[2] = tmp[2];
}

/* Performs rotation on 3x3 rotation matrix */
static void matrix_orthogonalize_and_rotate(double m[9], double rx, double ry, double rz)
{
    // Half of the dot product (error)
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

    // Vector cross product
    double prod[3] =
    {
        acc[1] * mag[2] - acc[2] * mag[1],
        acc[2] * mag[0] - acc[0] * mag[2],
        acc[0] * mag[1] - acc[1] * mag[0]
    };

    // Rotate
    m[0] = mag[0] + rz*prod[3] - ry*acc[6];
    m[1] = mag[1] + rz*prod[4] - ry*acc[7];
    m[2] = mag[2] + rz*prod[5] - ry*acc[8];
    m[3] = (rx*ry-rz)*mag[0] + (rx*ry*rz+1)*prod[3] + rx*acc[6];
    m[4] = (rx*ry-rz)*mag[1] + (rx*ry*rz+1)*prod[4] + rx*acc[7];
    m[5] = (rx*ry-rz)*mag[2] + (rx*ry*rz+1)*prod[5] + rx*acc[8];
    m[6] = (rx*rz+ry)*mag[0] + (ry*rz-rx)*prod[3] + acc[6];
    m[7] = (rx*rz+ry)*mag[1] + (ry*rz-rx)*prod[4] + acc[7];
    m[8] = (rx*rz+ry)*mag[2] + (ry*rz-rx)*prod[5] + acc[8];
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

    while(1)
    {
        struct driver_data data;
        double matrix[9] = { };
        int i;

        // Get data
        timerfd_select(imu->timerfd);
        if(!driver_read(imu->devfd, &data)) goto error;

        // Apply calibrations
        data.mag[0] -= imu->calib.mag_deviation_x;
        data.mag[1] -= imu->calib.mag_deviation_y;
        data.mag[2] -= imu->calib.mag_deviation_z;
        data.gyro[0] -= imu->calib.gyro_offset_x;
        data.gyro[1] -= imu->calib.gyro_offset_y;
        data.gyro[2] -= imu->calib.gyro_offset_z;
        vector_normalize(data.acc);
        vector_normalize(data.mag);
        vector_rotate_yz(data.mag, imu->calib.mag_inclination, imu->calib.mag_declination);
        INFO("Compass [%lf, %lf, %lf]", data.mag[0], data.mag[1], data.mag[2]);
        INFO("Gyroscope [%lf, %lf, %lf]", data.gyro[0], data.gyro[1], data.gyro[2]);
        INFO("Accelerometer [%lf, %lf, %lf]", data.acc[0], data.acc[1], data.acc[2]);

        // Integrate
        data.gyro[0] *= (double)TIMER_ITER_MS / 1000.0;
        data.gyro[1] *= (double)TIMER_ITER_MS / 1000.0;
        data.gyro[2] *= (double)TIMER_ITER_MS / 1000.0;

        // Update matrix
        matrix[0] = (1 - imu->calib.mag_weight) * matrix[0] + imu->calib.mag_weight * data.mag[0];
        matrix[1] = (1 - imu->calib.mag_weight) * matrix[1] + imu->calib.mag_weight * data.mag[1];
        matrix[2] = (1 - imu->calib.mag_weight) * matrix[2] + imu->calib.mag_weight * data.mag[2];
        matrix[6] = (1 - imu->calib.acc_weight) * matrix[6] + imu->calib.acc_weight * data.acc[0];
        matrix[7] = (1 - imu->calib.acc_weight) * matrix[7] + imu->calib.acc_weight * data.acc[1];
        matrix[8] = (1 - imu->calib.acc_weight) * matrix[8] + imu->calib.acc_weight * data.acc[2];
        vector_normalize(&matrix[0]);
        vector_normalize(&matrix[6]);
        matrix_orthogonalize_and_rotate(matrix, data.gyro[0], data.gyro[1], data.gyro[2]);
        pthread_mutex_lock(&imu->mutex);
        for(i = 0; i < 9; i++) imu->matrix[i] = matrix[i];
        pthread_mutex_unlock(&imu->mutex);
    }

error:
    WARN("Failed to read from driver");
    imu->is_alive = 0;
    return NULL;
}

int imu_load_calib(struct imucalib *calib, const char *source)
{
    DEBUG("imu_load_calib()");
    assert(calib != NULL);
    assert(source != NULL);

    FILE *f;
    if((f = fopen(source, "r")) == NULL)
    {
        WARN("Failed to open `%s`", source);
        return 0;
    }

    bzero(calib, sizeof(struct imucalib));
    char buf[BUFFER_SIZE];
    while(fgets(buf, BUFFER_SIZE, f) != NULL)
    {
        char *str = buf;

        // Skip empty lines and '#' comments
        while(*str == ' ') str++;
        if((*str == '\n') || (*str == '#')) continue;

        // Parse line
        if(sscanf(str, "gyro_offset_x = %lf", &calib->gyro_offset_x) != 1)
        if(sscanf(str, "gyro_offset_y = %lf", &calib->gyro_offset_y) != 1)
        if(sscanf(str, "gyro_offset_z = %lf", &calib->gyro_offset_z) != 1)
        if(sscanf(str, "mag_deviation_x = %lf", &calib->mag_deviation_x) != 1)
        if(sscanf(str, "mag_deviation_y = %lf", &calib->mag_deviation_y) != 1)
        if(sscanf(str, "mag_deviation_z = %lf", &calib->mag_deviation_z) != 1)
        if(sscanf(str, "mag_declination = %lf", &calib->mag_declination) != 1)
        if(sscanf(str, "mag_inclination = %lf", &calib->mag_inclination) != 1)
        if(sscanf(str, "mag_weight = %lf", &calib->mag_weight) != 1)
        if(sscanf(str, "acc_weight = %lf", &calib->acc_weight) != 1)
        {
            WARN("Parse error");
            return 0;
        }
    }

    INFO("Calibration(1/2) gyro_offset_x = %lf, gyro_offset_y = %lf, gyro_offset_z = %lf, mag_deviation_x = %lf, mag_deviation_y = %lf, mag_deviation_z = %lf",
         calib->gyro_offset_x, calib->gyro_offset_y, calib->gyro_offset_z, calib->mag_deviation_x, calib->mag_deviation_y, calib->mag_deviation_z);
    INFO("Calibration(2/2) mag_declination = %lf, mag_inclination = %lf, mag_weight = %lf, acc_weight = %lf",
         calib->mag_declination, calib->mag_inclination, calib->mag_weight, calib->acc_weight);
    return 1;
}

imu_t *imu_open(const char *devname, const struct imucalib *calib)
{
    DEBUG("imu_open()");
    assert(devname != NULL);
    assert(calib != NULL);

    // Initialize state structure
    imu_t *imu = calloc(1, sizeof(struct _imu));
    if(imu == NULL)
    {
        WARN("Cannot allocate memory");
        return NULL;
    }
    memcpy(&imu->calib, calib, sizeof(struct imucalib));

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
