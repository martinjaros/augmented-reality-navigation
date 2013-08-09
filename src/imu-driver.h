/**
 * @file
 * @brief       Inertial measurement unit - internal drivers
 * @author      Martin Jaros <xjaros32@stud.feec.vutbr.cz>
 *
 * @section LICENSE
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
 *
 * @section DESCRIPTION
 * These are internal I2C drivers for the IMU module.
 * Supported devices: ITG-3200, AK8975, BMA-150
 *
 */

#ifndef IMU_DRIVER_H
#define IMU_DRIVER_H

/**
 * @brief Measurement data from the driver
 */
struct driver_data
{
    /**
     * @brief Gyroscope data (x, y, z)
     */
    double gyro[3];

    /**
     * @brief Compass data (x, y, z)
     */
    double mag[3];

    /**
     * @brief Accelerometer data (x, y, z)
     */
    double acc[3];
};

/**
 * @brief Initializes the driver
 * @param fd File descriptor of the I2C device
 * @return 1 on success, 0 on failure
 */
int driver_init(int fd);

/**
 * @brief Reads data from the devices
 * @param fd File descriptor of the I2C device
 * @param[out] data Measurement results
 * @return 1 on success, 0 on failure
 */
int driver_read(int fd, struct driver_data *data);

#endif /* IMU_DRIVER_H */
