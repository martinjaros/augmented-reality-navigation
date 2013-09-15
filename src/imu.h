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
 * Measurements are done on separate thread.
 * @note All functions do not block
 *
 * Example:
 * @code
 * int main()
 * {
 *     float vec3[] = { 0, 0, 0 };
 *     imu_t *imu = imu_init("/dev/iio:device0", vec3, vec3, 0, 0, 0.2, 0.2);
 *
 *     while(1)
 *     {
 *         float roll, pitch, yaw;
 *         imu_get_attitude(imu, &roll, &pitch, &yaw);
 *
 *         // TODO: Do some processing here
 *
 *         sleep(1);
 *     }
 *
 *     imu_free(imu);
 * }
 * @endcode
 */

#ifndef IMU_H
#define IMU_H

/**
 * @brief Internal object
 */
typedef struct _imu imu_t;

/**
 * @brief Initializes IMU device
 * @param device IIO device name eg. "/dev/iio:device0"
 * @param gyro_scale Gyroscope scale coefficient
 * @param gyro_offset Gyroscope offset in radians per second
 * @param mag_declination Magnetic declination
 * @param mag_inclination Magnetic inclination
 * @param mag_weight Compass measurement weight
 * @param acc_weight Accelerometer measurement weight
 * @returns Internal object or NULL on error
 */
imu_t *imu_init(const char *device, float gyro_scale, float gyro_offset[3], float mag_declination, float mag_inclination, float mag_weight, float acc_weight);

/**
 * @brief Gets attitude information
 * @param imu Object as returned by `imu_init()`
 * @param[out] roll Roll angle in radians
 * @param[out] pitch Pitch angle in radians
 * @param[out] yaw Yaw angle in radians
 * @return 1 on success, 0 otherwise
 */
void imu_get_attitude(imu_t *imu, float *roll, float *pitch, float *yaw);

/**
 * @brief Releases resources
 * @param imu Object as returned by `imu_init()`
 */
void imu_free(imu_t *imu);

#endif /* IMU_H */
