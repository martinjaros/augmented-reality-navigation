/**
 * @file
 * @brief       Inertial measurement unit - configuration
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
 * These are configuration definitions for inertial measurement unit.
 */

#ifndef IMU_CONFIG_H
#define IMU_CONFIG_H

/**
 * @brief IMU configuration structure
 */
struct imu_config
{
    /**
     * @brief Gyroscope offset
     */
    float gyro_offset[3];

    /**
     * Gyroscope measurement weight (0 - 1)
     */
    float gyro_weight;

    /**
     * @brief Gyroscope measurement scale
     */
    float gyro_scale;

    /**
     * @brief Accelerometer measurement scale
     */
    float acc_scale;
};

#endif /* IMU_CONFIG_H */
