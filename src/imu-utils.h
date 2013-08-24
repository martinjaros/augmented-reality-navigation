/**
 * @file
 * @brief       Inertial measurement unit - calibration utilities
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
 * These are utilities for loading and generating calibration configurations for IMU devices.
 *
 */

#ifndef IMU_UTILS_H
#define IMU_UTILS_H

/**
 * @brief IMU calibration data
 */
struct imucalib
{
    /**
     * @brief Gyroscope X axis zero offset (radian per second)
     */
    double gyro_offset_x;

    /**
     * @brief Gyroscope Y axis zero offset (radian per second)
     */
    double gyro_offset_y;

    /**
     * @brief Gyroscope Z axis zero offset (radian per second)
     */
    double gyro_offset_z;

    /**
     * @brief Compass deviation in X axis (microtesla)
     */
    double mag_deviation_x;

    /**
     * @brief Compass deviation in Y axis (microtesla)
     */
    double mag_deviation_y;

    /**
     * @brief Compass deviation in Z axis (microtesla)
     */
    double mag_deviation_z;

    /**
     * @brief Compass declination (radian)
     */
    double mag_declination;

    /**
     * @brief Compass inclination (radian)
     */
    double mag_inclination;

    /**
     * @brief Relative weight of the compass measurements
     */
    double mag_weight;

    /**
     * @brief Relative weight of the accelerometer measurements
     */
    double acc_weight;
};

/**
 * @brief Loads calibration data from a file
 * @param[out] calib Calibration data
 * @param source Source file name
 * @return 1 on success, 0 otherwise
 */
int imu_calib_load(struct imucalib *calib, const char *source);

/**
 * @brief Saves calibration data to a file
 * @param[out] calib Calibration data
 * @param dest Destination file name
 * @return 1 on success, 0 otherwise
 */
int imu_calib_save(const struct imucalib *calib, const char *dest);

/**
 * @brief Generates calibration data while interactively asking user
 * @param[out] calib Calibration data
 * @param devname I2C device name
 * @return 1 on success, 0 otherwise
 */
int imu_calib_generate(struct imucalib *calib, const char *devname);

#endif /* IMU_UTILS_H */
