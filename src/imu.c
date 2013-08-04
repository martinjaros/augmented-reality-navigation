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
#include <stdint.h>
#include <assert.h>
#include <fcntl.h>
#include <unistd.h>
#include <math.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/timerfd.h>
#include <linux/i2c.h>
#include <linux/i2c-dev.h>

#include "debug.h"
#include "imu.h"

#ifndef M_PI
#define M_PI    3.14159265358979323846
#endif

/* Timer period */
#define TIMER_ITER_MS       10

/* Device definitions for ITG-3200 */
#define GYRO_ADDR           0x68
#define GYRO_REG            0x1D
#define GYRO_CONF           { 0x15, 0, (3 << 3) | 1, 0 }

#define GYRO_SCALE          (2000.0 / 180.0 * M_PI) / 32767.0 / 1000.0 * TIMER_ITER_MS
#define GYRO_CALIB_SKIP     2
#define GYRO_CALIB_STEPS    30

/* Device definitions for AK8975 */
#define MAG_ADDR            0x0C
#define MAG_REG             0x03
#define MAG_CONF            { 0x0A, 1 }

/* Device definitions for BMA-150 */
#define ACC_ADDR            0x70
#define ACC_REG             0x02

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

    unsigned char gyro_conf[] = GYRO_CONF;
    unsigned char mag_conf[] = MAG_CONF;

    struct i2c_msg msg[] =
    {
        // Write gyroscope configuration (continuous measurement)
        {
            .addr = GYRO_ADDR,
            .flags = 0,
            .len = sizeof(gyro_conf),
            .buf = gyro_conf
        },
        // Write compass configuration (single shot)
        {
            .addr = MAG_ADDR,
            .flags = 0,
            .len = sizeof(mag_conf),
            .buf = mag_conf
        }
    };

    struct i2c_rdwr_ioctl_data msgset;
    msgset.msgs = msg;
    msgset.nmsgs = sizeof(msg) / sizeof(struct i2c_msg);

    // Write data
    if(ioctl(fd, I2C_RDWR, &msgset) == -1)
    {
        WARN("I2C write fail");
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

    unsigned char gyro_buf[6] = { GYRO_REG };
    unsigned char acc_buf[6] = { ACC_REG };
    unsigned char mag_buf[6] = { MAG_REG };
    unsigned char mag_conf[] = MAG_CONF;

    struct i2c_msg msg[] =
    {
        // Read gyroscope measurement
        {
            .addr = GYRO_ADDR,
            .flags = I2C_M_RD,
            .len = sizeof(gyro_buf),
            .buf = gyro_buf
        },
        // Read compass measurement
        {
            .addr = MAG_ADDR,
            .flags = I2C_M_RD,
            .len = sizeof(mag_buf),
            .buf = mag_buf
        },
        // Reconfigure compass (single shot)
        {
            .addr = MAG_ADDR,
            .flags = 0,
            .len = sizeof(mag_conf),
            .buf = mag_conf
        },
        // Read accelerometer measurement
        {
            .addr = ACC_REG,
            .flags = I2C_M_RD,
            .len = sizeof(acc_buf),
            .buf = acc_buf
        }
    };

    struct i2c_rdwr_ioctl_data msgset;
    msgset.msgs = msg;
    msgset.nmsgs = sizeof(msg) / sizeof(struct i2c_msg);

    // Read data
    if(ioctl(imu->fd, I2C_RDWR, &msgset) == -1)
    {
        WARN("I2C read fail");
        return 0;
    }

    // Skip initial measurements
    if(imu->steps++ < 0) return 0;

    // Calibration steps
    if(imu->steps++ < GYRO_CALIB_STEPS)
    {
        imu->calib[0] += (int16_t)((uint16_t)gyro_buf[0] << 8 | (uint16_t)gyro_buf[1]);
        imu->calib[1] += (int16_t)((uint16_t)gyro_buf[2] << 8 | (uint16_t)gyro_buf[3]);
        imu->calib[2] += (int16_t)((uint16_t)gyro_buf[4] << 8 | (uint16_t)gyro_buf[5]);
        return 0;
    }
    else if(imu->steps++ == GYRO_CALIB_STEPS)
    {
        // Finalize calibration
        imu->calib[0] /= GYRO_CALIB_STEPS;
        imu->calib[1] /= GYRO_CALIB_STEPS;
        imu->calib[2] /= GYRO_CALIB_STEPS;

        // Initialize rotation matrix
        imu->matrix[0] = (int16_t)((uint16_t)mag_buf[0] | (uint16_t)mag_buf[1] << 8);
        imu->matrix[1] = (int16_t)((uint16_t)mag_buf[2] | (uint16_t)mag_buf[3] << 8);
        imu->matrix[2] = (int16_t)((uint16_t)mag_buf[4] | (uint16_t)mag_buf[5] << 8);

        uint16_t ax = (uint16_t)mag_buf[0] >> 6 | (uint16_t)mag_buf[1] << 2;
        uint16_t ay = (uint16_t)mag_buf[2] >> 6 | (uint16_t)mag_buf[3] << 2;
        uint16_t az = (uint16_t)mag_buf[4] >> 6 | (uint16_t)mag_buf[5] << 2;
        imu->matrix[6] = (int16_t)(ax > 0x1FF ? ax - 0x400 : ax);
        imu->matrix[7] = (int16_t)(ay > 0x1FF ? ay - 0x400 : ay);
        imu->matrix[8] = (int16_t)(az > 0x1FF ? az - 0x400 : az);
        vector_normalize(&imu->matrix[0]);
        vector_normalize(&imu->matrix[6]);
        matrix_orthogonalize(imu->matrix);

        INFO("IMU calibration finished");
    }
    else
    {
        // Get orientation vector from compass
        double mag[3] =
        {
            (short)(mag_buf[0] | mag_buf[1] << 8),
            (short)(mag_buf[2] | mag_buf[3] << 8),
            (short)(mag_buf[4] | mag_buf[5] << 8)
        };
        vector_normalize(mag);
        INFO("Compass readings [%lf, %lf, %lf]", mag[0], mag[1], mag[2]);

        // Get orientation vector from accelerometer
        uint16_t ax = (uint16_t)mag_buf[0] >> 6 | (uint16_t)mag_buf[1] << 2;
        uint16_t ay = (uint16_t)mag_buf[2] >> 6 | (uint16_t)mag_buf[3] << 2;
        uint16_t az = (uint16_t)mag_buf[4] >> 6 | (uint16_t)mag_buf[5] << 2;
        double acc[3] =
        {
            (int16_t)(ax > 0x1FF ? ax - 0x400 : ax),
            (int16_t)(ay > 0x1FF ? ay - 0x400 : ay),
            (int16_t)(az > 0x1FF ? az - 0x400 : az)
        };
        vector_normalize(acc);
        INFO("Accelerometer readings [%lf, %lf, %lf]", acc[0], acc[1], acc[2]);

        // Calculate weighted average
        imu->matrix[0] = 0.5 * imu->matrix[0] + 0.5 * mag[0];
        imu->matrix[1] = 0.5 * imu->matrix[1] + 0.5 * mag[1];
        imu->matrix[2] = 0.5 * imu->matrix[2] + 0.5 * mag[2];
        imu->matrix[6] = 0.5 * imu->matrix[6] + 0.5 * acc[0];
        imu->matrix[7] = 0.5 * imu->matrix[7] + 0.5 * acc[1];
        imu->matrix[8] = 0.5 * imu->matrix[8] + 0.5 * acc[2];
        vector_normalize(&imu->matrix[0]);
        vector_normalize(&imu->matrix[6]);
        matrix_orthogonalize(imu->matrix);
    }

    // Get displacement angles from gyroscope and rotate matrix
    double rx = ((short)(gyro_buf[0] << 8 | gyro_buf[1]) - imu->calib[0]) * GYRO_SCALE;
    double ry = ((short)(gyro_buf[2] << 8 | gyro_buf[3]) - imu->calib[1]) * GYRO_SCALE;
    double rz = ((short)(gyro_buf[4] << 8 | gyro_buf[5]) - imu->calib[2]) * GYRO_SCALE;
    matrix_rotate(imu->matrix, rx, ry, rz);
    INFO("Gyroscope readings [%lf, %lf, %lf]", rx, ry, rz);

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
