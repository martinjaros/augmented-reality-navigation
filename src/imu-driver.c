/*
 * Inertial measurement unit - internal drivers
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
#include <math.h>
#include <sys/ioctl.h>
#include <linux/i2c.h>
#include <linux/i2c-dev.h>

#include "debug.h"
#include "imu-driver.h"

/* Device definitions */
#define ITG3200_ADDR           0x68
#define ITG3200_REG            0x1D
#define ITG3200_CONF           { 0x15, 0, (3 << 3) | 1, 0 }
#define ITG3200_SCALE          M_PI / 180.0 / 14.375

#define AK8975_ADDR            0x0C
#define AK8975_REG             0x03
#define AK8975_CONF            { 0x0A, 1 }
#define AK8975_SCALE           0.3

#define BMA150_ADDR            0x38
#define BMA150_REG             0x02
#define BMA150_SCALE           1.0 / 128.0

/* Performs I2C block write */
static int i2c_write(int fd, uint16_t addr, uint16_t len, uint8_t *buf)
{
    struct i2c_msg msg =
    {
        .addr = addr,
        .flags = 0,
        .len = len,
        .buf = buf
    };

    struct i2c_rdwr_ioctl_data msgset;
    msgset.msgs = &msg;
    msgset.nmsgs = 1;

    if(ioctl(fd, I2C_RDWR, &msgset) == -1) return 0;

    return 1;
}

/* Performs I2C one byte write and then block read */
static int i2c_read(int fd, uint16_t addr, uint16_t len, uint8_t *buf)
{
    struct i2c_msg msg[] =
    {
        {
            .addr = addr,
            .flags = 0,
            .len = 1,
            .buf = buf
        },
        {
            .addr = addr,
            .flags = I2C_M_RD,
            .len = len,
            .buf = buf
        }
    };

    struct i2c_rdwr_ioctl_data msgset;
    msgset.msgs = msg;
    msgset.nmsgs = 2;

    if(ioctl(fd, I2C_RDWR, &msgset) == -1) return 0;

    return 1;
}

int driver_init(int fd)
{
    DEBUG("driver_init()");

    unsigned char gyro_conf[] = ITG3200_CONF;
    unsigned char mag_conf[] = AK8975_CONF;

    if(!i2c_write(fd, ITG3200_ADDR, sizeof(gyro_conf), gyro_conf) ||
       !i2c_write(fd, AK8975_ADDR, sizeof(mag_conf), mag_conf))
    {
        WARN("I2C transmission failed");
        return 0;
    }

    return 1;
}

int driver_read(int fd, struct driver_data *data)
{
    DEBUG("driver_read()");
    assert(data != NULL);

    unsigned char gyro_buf[6] = { ITG3200_REG };
    unsigned char acc_buf[6] = { BMA150_REG };
    unsigned char mag_buf[6] = { AK8975_REG };
    unsigned char mag_conf[] = AK8975_CONF;

    if(!i2c_read(fd, ITG3200_ADDR, sizeof(gyro_buf), gyro_buf) ||
       !i2c_read(fd, ITG3200_ADDR, sizeof(mag_buf), mag_buf) ||
       !i2c_write(fd, AK8975_ADDR, sizeof(mag_conf), mag_conf) ||
       !i2c_read(fd, ITG3200_ADDR, sizeof(acc_buf), acc_buf))
    {
        WARN("I2C transmission failed");
        return 0;
    }

    uint16_t ax = (uint16_t)acc_buf[0] >> 6 | (uint16_t)acc_buf[1] << 2;
    uint16_t ay = (uint16_t)acc_buf[2] >> 6 | (uint16_t)acc_buf[3] << 2;
    uint16_t az = (uint16_t)acc_buf[4] >> 6 | (uint16_t)acc_buf[5] << 2;
    data->acc[0] = (int16_t)(ax > 0x1FF ? ax - 0x400 : ax) * BMA150_SCALE;
    data->acc[1] = (int16_t)(ay > 0x1FF ? ay - 0x400 : ay) * BMA150_SCALE;
    data->acc[2] = (int16_t)(az > 0x1FF ? az - 0x400 : az) * BMA150_SCALE;

    data->mag[0] = (int16_t)((uint16_t)mag_buf[0] | (uint16_t)mag_buf[1] << 8) * AK8975_SCALE;
    data->mag[1] = (int16_t)((uint16_t)mag_buf[2] | (uint16_t)mag_buf[3] << 8) * AK8975_SCALE;
    data->mag[2] = (int16_t)((uint16_t)mag_buf[4] | (uint16_t)mag_buf[5] << 8) * AK8975_SCALE;

    data->gyro[0] = (int16_t)((uint16_t)gyro_buf[0] << 8 | (uint16_t)gyro_buf[1]) * ITG3200_SCALE;
    data->gyro[1] = (int16_t)((uint16_t)gyro_buf[2] << 8 | (uint16_t)gyro_buf[3]) * ITG3200_SCALE;
    data->gyro[2] = (int16_t)((uint16_t)gyro_buf[4] << 8 | (uint16_t)gyro_buf[5]) * ITG3200_SCALE;

    return 1;
}
