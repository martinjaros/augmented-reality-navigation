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
 *     struct imu_config config =
 *     {
 *         .gyro_offset = { 0, 0, 0 },
 *         .gyro_weight = .8,
 *         .gyro_scale = 1,
 *     };
 *
 *     imu_t *imu = imu_init("/dev/iio:device0", &config);
 *
 *     while(1)
 *     {
 *         float attitude[3];
 *         imu_get_attitude(imu, attitude);
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

#include "imu-config.h"

/**
 * @brief Internal object
 */
typedef struct _imu imu_t;

/**
 * @brief Initializes IMU device
 * @param device IIO device name eg. "/dev/iio:device0"
 * @param config Pointer to IMU configuration structure
 * @returns Internal object or NULL on error
 */
imu_t *imu_init(const char *device, const struct imu_config *config);

/**
 * @brief Gets attitude information
 * @param imu Object as returned by `imu_init()`
 * @param[out] attitude Roll, pitch and yaw angles in radians
 */
void imu_get_attitude(imu_t *imu, float attitude[3]);

/**
 * @brief Releases resources
 * @param imu Object as returned by `imu_init()`
 */
void imu_free(imu_t *imu);

#endif /* IMU_H */
