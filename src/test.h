/**
 * @file
 * @brief       ARNav test suite
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
 * Augmented reality navigation test suite
 */

#ifndef TEST_H
#define TEST_H

#include <stdint.h>

/**
 * @brief Tests graphics module
 * @param font Font file path to use
 * @param width Requested window width
 * @param height Requested window height
 * @param loops Number of loops
 * @return 1 on success, 0 on failure
 */
int test_graphics(const char *font, uint16_t width, uint16_t height, int loops);

/**
 * @brief Tests GPS module
 * @param devname Serial device name eg. "/dev/ttyS0"
 * @param loops Number of loops
 * @return 1 on success, 0 on failure
 */
int test_gps(const char *devname, int loops);

/**
 * @brief Tests IMU module
 * @param devname Name of the I2C bus eg. "/dev/i2c-0"
 * @param calibname Name of the calibration file
 * @param loops Number of loops
 * @return 1 on success, 0 on failure
 */
int test_imu(const char *devname, const char *calibname, int loops);

/**
 * @brief Tests capture module
 * @param devname Device name eg. "/dev/video0"
 * @param width Requested frame width
 * @param height Requested frame height
 * @param loops Number of loops
 * @return 1 on success, 0 on failure
 */
int test_capture(const char *devname, uint16_t width, uint16_t height, int loops);

#endif /* TEST_H */
