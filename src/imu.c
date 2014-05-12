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

#include <assert.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <math.h>
#include <stdlib.h>
#include <pthread.h>

#include "debug.h"
#include "imu.h"

#define EARTH_GRAVITY 9.81

/* Byte-order swap */
#define SWAPI16(x) (int16_t)((((uint16_t)x & 0x00ff) << 8) | (((uint16_t)x & 0xff00) >> 8))

struct _imu
{
    int fd;
    float dcm[9];
    uint64_t timestamp;

    uint64_t reftime;
    float accsum[3];

    pthread_t thread;
    pthread_mutex_t mutex;
    const struct imu_config *config;
};

/* IIO buffer structure */
struct __attribute__((__packed__)) buffer
{
    int16_t acc[3], gyro[3], mag[3];
    uint64_t timestamp;
};

/* IIO buffer dequantization */
inline void dequantize(const struct imu_config *config, struct buffer buf, float gyro[3], float mag[3], float acc[3])
{
    gyro[0] = ((float)SWAPI16(buf.gyro[0]) + config->gyro_offset[0]) * config->gyro_scale;
    gyro[1] = ((float)SWAPI16(buf.gyro[1]) + config->gyro_offset[1]) * config->gyro_scale;
    gyro[2] = ((float)SWAPI16(buf.gyro[2]) + config->gyro_offset[2]) * config->gyro_scale;
    mag[0] = (float)SWAPI16(buf.mag[0]);
    mag[1] = (float)SWAPI16(buf.mag[1]);
    mag[2] = (float)SWAPI16(buf.mag[2]);
    acc[0] = (float)SWAPI16(buf.acc[0]) * config->acc_scale * EARTH_GRAVITY;
    acc[1] = (float)SWAPI16(buf.acc[1]) * config->acc_scale * EARTH_GRAVITY;
    acc[2] = (float)SWAPI16(buf.acc[2]) * config->acc_scale * EARTH_GRAVITY;
}

/* Vector multiply, res = a x b */
inline void vect_mult(float a[3], float b[3], float res[3])
{
    res[0] = a[1] * b[2] - a[2] * b[1];
    res[1] = a[2] * b[0] - a[0] * b[2];
    res[2] = a[0] * b[1] - a[1] * b[0];
}

/* Vector normalize, res = a / abs(a) */
inline void vect_norm(float a[3], float res[3])
{
    float len = sqrtf(a[0] * a[0] + a[1] * a[1] + a[2] * a[2]);
    res[0] = a[0] / len;
    res[1] = a[1] / len;
    res[2] = a[2] / len;
}

static void *worker(void *arg)
{
    INFO("Thread started");
    float gyro[3], mag[3], acc[3];
    struct buffer buf;
    imu_t *imu = (imu_t*)arg;

    // Read first buffer
    if(read(imu->fd, &buf, sizeof(struct buffer)) != sizeof(struct buffer)) goto finalize;

    // Initialize DCM
    pthread_mutex_lock(&imu->mutex);
    dequantize(imu->config, buf, gyro, mag, acc);
    INFO("Gyro [%f, %f, %f], Mag [%f, %f, %f], Acc [%f, %f, %f]",
         gyro[0], gyro[1], gyro[2], mag[0], mag[1], mag[2], acc[0], acc[1], acc[2]);

    vect_norm(mag, mag);
    vect_norm(acc, &imu->dcm[6]);
    vect_mult(&imu->dcm[6], mag, &imu->dcm[3]);
    vect_mult(&imu->dcm[3], &imu->dcm[6], &imu->dcm[0]);
    imu->reftime = imu->timestamp = buf.timestamp;
    pthread_mutex_unlock(&imu->mutex);

    while(read(imu->fd, &buf, sizeof(struct buffer)) == sizeof(struct buffer))
    {
        pthread_mutex_lock(&imu->mutex);
        dequantize(imu->config, buf, gyro, mag, acc);
        INFO("Gyro [%f, %f, %f], Mag [%f, %f, %f], Acc [%f, %f, %f]",
             gyro[0], gyro[1], gyro[2], mag[0], mag[1], mag[2], acc[0], acc[1], acc[2]);

        // Rotate to global frame
        imu->accsum[0] += imu->dcm[0] * acc[0] + imu->dcm[1] * acc[1] + imu->dcm[2] * acc[2];
        imu->accsum[1] += imu->dcm[3] * acc[0] + imu->dcm[4] * acc[1] + imu->dcm[5] * acc[2];
        imu->accsum[2] += imu->dcm[6] * acc[0] + imu->dcm[7] * acc[1] + imu->dcm[8] * acc[2] - EARTH_GRAVITY;

        // Integrate
        float diff = (float)(buf.timestamp - imu->timestamp) / 1e9;
        gyro[0] *= diff;
        gyro[1] *= diff;
        gyro[2] *= diff;
        imu->timestamp = buf.timestamp;
        imu->dcm[0] = imu->dcm[0] + imu->dcm[3] * (gyro[0] * gyro[1] + gyro[2]) + imu->dcm[6] * (gyro[0] * gyro[2] - gyro[1]);
        imu->dcm[1] = imu->dcm[1] + imu->dcm[4] * (gyro[0] * gyro[1] + gyro[2]) + imu->dcm[7] * (gyro[0] * gyro[2] - gyro[1]);
        imu->dcm[2] = imu->dcm[2] + imu->dcm[5] * (gyro[0] * gyro[1] + gyro[2]) + imu->dcm[8] * (gyro[0] * gyro[2] - gyro[1]);
        imu->dcm[3] = imu->dcm[0] * -gyro[2] + imu->dcm[3] * (1 - gyro[0] * gyro[1] * gyro[2]) + imu->dcm[6] * (gyro[0] + gyro[1] * gyro[2]);
        imu->dcm[4] = imu->dcm[1] * -gyro[2] + imu->dcm[4] * (1 - gyro[0] * gyro[1] * gyro[2]) + imu->dcm[7] * (gyro[0] + gyro[1] * gyro[2]);
        imu->dcm[5] = imu->dcm[2] * -gyro[2] + imu->dcm[5] * (1 - gyro[0] * gyro[1] * gyro[2]) + imu->dcm[8] * (gyro[0] + gyro[1] * gyro[2]);
        imu->dcm[6] = imu->dcm[0] * gyro[1] + imu->dcm[3] * -gyro[0] + imu->dcm[6];
        imu->dcm[7] = imu->dcm[1] * gyro[1] + imu->dcm[4] * -gyro[0] + imu->dcm[7];
        imu->dcm[8] = imu->dcm[2] * gyro[1] + imu->dcm[5] * -gyro[0] + imu->dcm[8];

        // Compute average
        float tmp[3];
        vect_norm(mag, mag);
        vect_norm(acc, acc);
        vect_mult(acc, mag, tmp);
        vect_mult(tmp, acc, mag);
        imu->dcm[0] = imu->config->gyro_weight * imu->dcm[0] + (1 - imu->config->gyro_weight) * mag[0];
        imu->dcm[1] = imu->config->gyro_weight * imu->dcm[1] + (1 - imu->config->gyro_weight) * mag[1];
        imu->dcm[2] = imu->config->gyro_weight * imu->dcm[2] + (1 - imu->config->gyro_weight) * mag[2];
        imu->dcm[3] = imu->config->gyro_weight * imu->dcm[3] + (1 - imu->config->gyro_weight) * tmp[0];
        imu->dcm[4] = imu->config->gyro_weight * imu->dcm[4] + (1 - imu->config->gyro_weight) * tmp[1];
        imu->dcm[5] = imu->config->gyro_weight * imu->dcm[5] + (1 - imu->config->gyro_weight) * tmp[2];
        imu->dcm[6] = imu->config->gyro_weight * imu->dcm[6] + (1 - imu->config->gyro_weight) * acc[0];
        imu->dcm[7] = imu->config->gyro_weight * imu->dcm[7] + (1 - imu->config->gyro_weight) * acc[1];
        imu->dcm[8] = imu->config->gyro_weight * imu->dcm[8] + (1 - imu->config->gyro_weight) * acc[2];
        pthread_mutex_unlock(&imu->mutex);
    }

finalize:
    ERROR("Broken pipe");
    return NULL;
}

imu_t *imu_init(const char *device, const struct imu_config *config)
{
    DEBUG("imu_init()");
    assert(device != 0);
    assert(config != 0);

    imu_t *imu = malloc(sizeof(struct _imu));
    assert(imu != 0);

    imu->config = config;
    imu->dcm[0] = 1;
    imu->dcm[1] = 0;
    imu->dcm[2] = 0;
    imu->dcm[3] = 0;
    imu->dcm[4] = 1;
    imu->dcm[5] = 0;
    imu->dcm[6] = 0;
    imu->dcm[7] = 0;
    imu->dcm[8] = 1;
    imu->accsum[0] = 0;
    imu->accsum[1] = 0;
    imu->accsum[2] = 0;
    imu->reftime = 0;

    // Open device
    if((imu->fd = open(device, O_RDONLY | O_NOCTTY)) == -1)
    {
        WARN("Failed to open `%s`", device);
        free(imu);
        return NULL;
    }

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

void imu_get_attitude(imu_t *imu, float attitude[3])
{
    DEBUG("imu_get_attitude()");
    assert(imu != 0);
    assert(attitude != 0);

    pthread_mutex_lock(&imu->mutex);
    attitude[0] = -atan2f(imu->dcm[7], imu->dcm[8]);
    attitude[1] = asinf(imu->dcm[6]);
    attitude[2] = -atan2(imu->dcm[3], imu->dcm[0]);
    pthread_mutex_unlock(&imu->mutex);
}

void imu_get_acceleration(imu_t *imu, float accsum[3], float *difftime)
{
    DEBUG("imu_get_acceleration()");
    assert(imu != 0);
    assert(accsum != 0);
    assert(difftime != 0);

    pthread_mutex_lock(&imu->mutex);
    accsum[0] = imu->accsum[0];
    accsum[1] = imu->accsum[1];
    accsum[2] = imu->accsum[2];
    imu->accsum[0] = 0;
    imu->accsum[1] = 0;
    imu->accsum[2] = 0;
    *difftime = (float)(imu->timestamp - imu->reftime) / 1e9;
    imu->reftime = imu->timestamp;
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
