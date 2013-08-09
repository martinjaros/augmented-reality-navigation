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
 * You should use `select()` on file descriptor returned by `imu_timer_create()`
 * to synchronize before reading.
 * @note All functions do not block
 *
 * Example:
 * @code
 * int main()
 * {
 *     imu_t *imu = imu_open("/dev/i2c-0");
 *
 *     int fd = imu_timer_create();
 *
 *     while(1)
 *     {
 *         fd_set fds;
 *         FD_ZERO(&fds);
 *         FD_SET(fd, &fds);
 *
 *         select(fd + 1, &fds, NULL, NULL, NULL);
 *
 *         struct attdinfo attd;
 *         if(imu_read(imu, &attd))
 *         {
 *             // TODO: Do some processing here
 *         }
 *     }
 *
 *     imu_close(imu, fd);
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
struct attdinfo
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
 * @brief Creates timer to synchronize device access
 * @return File descriptor of the created timer or -1 on error
 */
int imu_timer_create();

/**
 * @brief Reads data from devices
 * @param imu State variable as returned by `imu_open()`
 * @param[out] attd Pointer to structure where to output results
 * @return 1 if the structure was modified, 0 otherwise
 */
int imu_read(imu_t *imu, struct attdinfo *attd);

/**
 * @brief Closes the device
 * @param imu State variable as returned by `imu_open()`
 * @param fd File descriptor as returned by `imu_timer()`
 */
void imu_close(imu_t *imu, int fd);

#endif /* IMU_H */
