/**
 * @file
 * @brief       Inertial measurement unit
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
 * This is an inertial measurement unit providing attitude data.
 * Measurements are done on separate thread
 * @note All functions do not block
 *
 * Example:
 * @code
 * int main()
 * {
 *     imu_t *imu = imu_open("/dev/i2c-0");
 *
 *     while(1)
 *     {
 *         struct attdinfo attd;
 *         if(imu_read(imu, &attd))
 *         {
 *             // TODO: Do some processing here
 *         }
 *
 *         sleep(1);
 *     }
 *
 *     imu_close(imu);
 * }
 * @endcode
 */

#ifndef IMU_H
#define IMU_H

/**
 * @brief IMU state variable
 */
typedef struct _imu imu_t;

/**
 * @brief Attitude information
 */
struct imudata
{
    /**
     * @brief Roll angle in radians
     */
    double roll;

    /**
     * @brief Pitch angle in radians
     */
    double pitch;

    /**
     * @brief Yaw angle in radians
     */
    double yaw;
};

/**
 * @brief Opens I2C bus and initializes the devices
 * @param devname Name of the I2C bus eg. "/dev/i2c-0"
 * @returns Internal state variable or NULL on error
 */
imu_t *imu_open(const char *devname);

/**
 * @brief Reads data from devices
 * @param imu State variable as returned by `imu_open()`
 * @param[out] data Pointer to structure where to output results
 * @return 1 on success, 0 otherwise
 */
int imu_read(imu_t *imu, struct imudata *data);

/**
 * @brief Closes the device
 * @param imu State variable as returned by `imu_open()`
 */
void imu_close(imu_t *imu);

#endif /* IMU_H */
