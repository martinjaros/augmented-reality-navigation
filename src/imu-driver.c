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
#define ITG3200_SCALE          (2000.0 / 180.0 * M_PI) / 32767.0 / 1000.0

#define AK8975_ADDR            0x0C
#define AK8975_REG             0x03
#define AK8975_CONF            { 0x0A, 1 }

#define BMA150_ADDR            0x70
#define BMA150_REG             0x02

int driver_init(int fd)
{
    DEBUG("driver_init()");

    unsigned char gyro_conf[] = ITG3200_CONF;
    unsigned char mag_conf[] = AK8975_CONF;

    struct i2c_msg msg[] =
    {
        // Write ITG-3200 configuration (continuous measurement)
        {
            .addr = ITG3200_ADDR,
            .flags = 0,
            .len = sizeof(gyro_conf),
            .buf = gyro_conf
        },
        // Write AK8975 configuration (single shot)
        {
            .addr = AK8975_ADDR,
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

    struct i2c_msg msg[] =
    {
        // Read gyroscope measurement
        {
            .addr = ITG3200_ADDR,
            .flags = 0,
            .len = 1,
            .buf = gyro_buf
        },
        {
            .addr = ITG3200_ADDR,
            .flags = I2C_M_RD,
            .len = sizeof(gyro_buf),
            .buf = gyro_buf
        },
        // Read compass measurement
        {
            .addr = AK8975_ADDR,
            .flags = 0,
            .len = 1,
            .buf = mag_buf
        },
        {
            .addr = AK8975_ADDR,
            .flags = I2C_M_RD,
            .len = sizeof(mag_buf),
            .buf = mag_buf
        },
        // Reconfigure compass (single shot)
        {
            .addr = AK8975_ADDR,
            .flags = 0,
            .len = sizeof(mag_conf),
            .buf = mag_conf
        },
        // Read accelerometer measurement
        {
            .addr = BMA150_ADDR,
            .flags = 0,
            .len = 1,
            .buf = acc_buf
        },
        {
            .addr = BMA150_ADDR,
            .flags = I2C_M_RD,
            .len = sizeof(acc_buf),
            .buf = acc_buf
        }
    };

    struct i2c_rdwr_ioctl_data msgset;
    msgset.msgs = msg;
    msgset.nmsgs = sizeof(msg) / sizeof(struct i2c_msg);

    // Read data
    if(ioctl(fd, I2C_RDWR, &msgset) == -1)
    {
        WARN("I2C read fail");
        return 0;
    }

    uint16_t ax = (uint16_t)acc_buf[0] >> 6 | (uint16_t)acc_buf[1] << 2;
    uint16_t ay = (uint16_t)acc_buf[2] >> 6 | (uint16_t)acc_buf[3] << 2;
    uint16_t az = (uint16_t)acc_buf[4] >> 6 | (uint16_t)acc_buf[5] << 2;
    data->acc[0] = (int16_t)(ax > 0x1FF ? ax - 0x400 : ax);
    data->acc[1] = (int16_t)(ay > 0x1FF ? ay - 0x400 : ay);
    data->acc[2] = (int16_t)(az > 0x1FF ? az - 0x400 : az);

    data->mag[0] = (int16_t)((uint16_t)mag_buf[0] | (uint16_t)mag_buf[1] << 8);
    data->mag[1] = (int16_t)((uint16_t)mag_buf[2] | (uint16_t)mag_buf[3] << 8);
    data->mag[2] = (int16_t)((uint16_t)mag_buf[4] | (uint16_t)mag_buf[5] << 8);

    data->gyro[0] = (int16_t)((uint16_t)gyro_buf[0] << 8 | (uint16_t)gyro_buf[1]) * ITG3200_SCALE;
    data->gyro[1] = (int16_t)((uint16_t)gyro_buf[2] << 8 | (uint16_t)gyro_buf[3]) * ITG3200_SCALE;
    data->gyro[2] = (int16_t)((uint16_t)gyro_buf[4] << 8 | (uint16_t)gyro_buf[5]) * ITG3200_SCALE;

    return 1;
}
