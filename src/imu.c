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
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <math.h>
#include <stdlib.h>
#include <pthread.h>

#include "debug.h"
#include "imu.h"

struct driver_data
{
    uint16_t gyro[3];
    uint16_t mag[3];
    uint16_t acc[3];
    uint64_t timestamp;
};

struct _imu
{
    int fd;
    pthread_t thread;
    pthread_mutex_t mutex;

    double matrix[9];
    float gyro_scale, gyro_offset[3], mag_declination, mag_inclination, mag_weight, acc_weight;
};

static void vector_normalize(float v[3])
{
    float len = sqrt(v[0]*v[0] + v[1]*v[1] + v[2]*v[2]);
    v[0] /= len;
    v[1] /= len;
    v[2] /= len;
}

static void vector_rotate_yz(float v[3], float ry, float rz)
{
    // Y rotation
    float sinr = sin(ry);
    float cosr = cos(ry);
    float tmp[3] =
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

static void matrix_orthogonalize_and_rotate(float m[9], float rx, float ry, float rz)
{
    // Half of the dot product (error)
    float dot_2 = (m[0]*m[6] + m[1]*m[7] + m[2]*m[8]) / 2;

    // Orthogonalized compass vector
    float mag[3] =
    {
        m[0] - dot_2 * m[6],
        m[1] - dot_2 * m[7],
        m[2] - dot_2 * m[8]
    };
    vector_normalize(mag);

    // Orthogonalized accelerometer vector
    float acc[3] =
    {
        m[6] - dot_2 * m[0],
        m[7] - dot_2 * m[1],
        m[8] - dot_2 * m[2]
    };
    vector_normalize(acc);

    // Vector cross product
    float prod[3] =
    {
        acc[1] * mag[2] - acc[2] * mag[1],
        acc[2] * mag[0] - acc[0] * mag[2],
        acc[0] * mag[1] - acc[1] * mag[0]
    };

    // Rotate
    m[0] = mag[0] + rz*prod[0] - ry*acc[0];
    m[1] = mag[1] + rz*prod[1] - ry*acc[1];
    m[2] = mag[2] + rz*prod[2] - ry*acc[2];
    m[3] = (rx*ry-rz)*mag[0] + (rx*ry*rz+1)*prod[0] + rx*acc[0];
    m[4] = (rx*ry-rz)*mag[1] + (rx*ry*rz+1)*prod[1] + rx*acc[1];
    m[5] = (rx*ry-rz)*mag[2] + (rx*ry*rz+1)*prod[2] + rx*acc[2];
    m[6] = (rx*rz+ry)*mag[0] + (ry*rz-rx)*prod[0] + acc[0];
    m[7] = (rx*rz+ry)*mag[1] + (ry*rz-rx)*prod[1] + acc[1];
    m[8] = (rx*rz+ry)*mag[2] + (ry*rz-rx)*prod[2] + acc[2];
}

static void *worker(void *arg)
{
    INFO("Thread started");
    imu_t *imu = (imu_t*)arg;

    uint64_t timestamp;
    float matrix[9];
    float gyro[3], mag[3], acc[3];

    struct driver_data data;
    ssize_t len;

    if((len = read(imu->fd, &data, sizeof(data))) != -1)
    if(len == sizeof(data))
    {
        // Initialize matrix
        timestamp = data.timestamp;
        vector_normalize(acc);
        vector_normalize(mag);
        vector_rotate_yz(mag, imu->mag_inclination, imu->mag_declination);
        matrix[0] = mag[0];
        matrix[1] = mag[1];
        matrix[2] = mag[2];
        matrix[6] = acc[0];
        matrix[7] = acc[1];
        matrix[8] = acc[2];

        while((len = read(imu->fd, &data, sizeof(data))) != -1)
        {
            if(len != sizeof(data)) break;

            // Apply scale
            float diff = (float)(data.timestamp - timestamp) / 1e9;
            gyro[0] = ((float)data.gyro[0] * imu->gyro_scale + imu->gyro_offset[0]) * diff;
            gyro[1] = ((float)data.gyro[1] * imu->gyro_scale + imu->gyro_offset[1]) * diff;
            gyro[2] = ((float)data.gyro[2] * imu->gyro_scale + imu->gyro_offset[2]) * diff;
            mag[0] = (float)data.mag[0];
            mag[1] = (float)data.mag[1];
            mag[2] = (float)data.mag[2];
            acc[0] = (float)data.acc[0];
            acc[1] = (float)data.acc[1];
            acc[2] = (float)data.acc[2];

            // Apply calibrations
            vector_normalize(acc);
            vector_normalize(mag);
            vector_rotate_yz(mag, imu->mag_inclination, imu->mag_declination);

            // Update matrix
            matrix[0] = (1 - imu->mag_weight) * matrix[0] + imu->mag_weight * mag[0];
            matrix[1] = (1 - imu->mag_weight) * matrix[1] + imu->mag_weight * mag[1];
            matrix[2] = (1 - imu->mag_weight) * matrix[2] + imu->mag_weight * mag[2];
            matrix[6] = (1 - imu->acc_weight) * matrix[6] + imu->acc_weight * acc[0];
            matrix[7] = (1 - imu->acc_weight) * matrix[7] + imu->acc_weight * acc[1];
            matrix[8] = (1 - imu->acc_weight) * matrix[8] + imu->acc_weight * acc[2];
            vector_normalize(&matrix[0]);
            vector_normalize(&matrix[6]);
            matrix_orthogonalize_and_rotate(matrix, gyro[0], gyro[1], gyro[2]);

            pthread_mutex_lock(&imu->mutex);
            int i;
            for(i = 0; i < 9; i++) imu->matrix[i] = matrix[i];
            pthread_mutex_unlock(&imu->mutex);
        }
    }

    ERROR("Broken pipe");
    return NULL;
}

imu_t *imu_init(const char *device, float gyro_scale, float gyro_offset[3], float mag_declination, float mag_inclination, float mag_weight, float acc_weight)
{
    DEBUG("imu_init()");
    assert(device != 0);
    assert(gyro_offset != 0);

    imu_t *imu = malloc(sizeof(struct _imu));
    assert(imu != 0);

    // Open device
    if((imu->fd = open(device, O_RDONLY | O_NOCTTY)) == -1)
    {
        WARN("Failed to open `%s`", device);
        free(imu);
        return NULL;
    }

    imu->matrix[0] = 1;
    imu->matrix[1] = 0;
    imu->matrix[2] = 0;
    imu->matrix[3] = 0;
    imu->matrix[4] = 1;
    imu->matrix[5] = 0;
    imu->matrix[6] = 0;
    imu->matrix[7] = 0;
    imu->matrix[8] = 1;
    imu->gyro_scale = gyro_scale;
    imu->gyro_offset[0] = gyro_offset[0];
    imu->gyro_offset[1] = gyro_offset[1];
    imu->gyro_offset[2] = gyro_offset[2];
    imu->mag_declination = mag_declination;
    imu->mag_inclination = mag_inclination;
    imu->mag_weight = mag_weight;
    imu->acc_weight = acc_weight;

    // Start worker thread
    if(pthread_mutex_init(&imu->mutex, NULL) || pthread_create(&imu->thread, NULL, worker, imu))
    {
        WARN("Failed to create thread");
        close(imu->fd);
        free(imu);
        return NULL;
    }

    return imu;
}

void imu_get_attitude(imu_t *imu, float *roll, float *pitch, float *yaw)
{
    DEBUG("imu_get_attitude()");
    assert(imu != 0);

    pthread_mutex_lock(&imu->mutex);
    if(roll) *roll = atan2(imu->matrix[5], imu->matrix[8]);
    if(pitch) *pitch = asin(imu->matrix[2]);
    if(yaw) *yaw = atan2(imu->matrix[1], imu->matrix[0]);
    pthread_mutex_unlock(&imu->mutex);
}

void imu_free(imu_t *imu)
{
    DEBUG("imu_free()");
    assert(imu != 0);

    pthread_cancel(imu->thread);
    pthread_join(imu->thread, NULL);
    pthread_mutex_destroy(&imu->mutex);
    close(imu->fd);
    free(imu);
}
