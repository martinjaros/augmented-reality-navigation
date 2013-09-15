/**
 * @file
 * @brief       ARNav application
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
 * Augmented reality navigation application
 */

#ifndef APPLICATION_H
#define APPLICATION_H

#include <stdint.h>

/**
 * @brief Application configuration structure
 */
struct config
{
    /************* APPLICATION *************/

    /**
     * @brief Landmark database file name
     */
    char *app_landmarks_file;

    /**
     * @brief Native window id
     */
    unsigned long app_window_id;


    /************* VIDEO *************/

    /**
     * @brief Device file name
     */
    char *video_device;

    /**
     * @brief Frame width in pixels
     */
    uint32_t video_width;

    /**
     * @brief Frame height in pixels
     */
    uint32_t video_height;

    /**
     * @brief Pixel format in fourcc notation
     */
    char video_format[4];

    /**
     * @brief Interlacing
     */
    uint8_t video_interlace;

    /**
     * @brief Horizontal field of view in radians
     */
    float video_hfov;

    /**
     * @brief Vertical field of view in radians
     */
    float video_vfov;


    /************* GRAPHICS *************/

    /**
     * @brief Font file name
     */
    char *graphics_font_file;

    /**
     * @brief Font color for level 1 (RGBA)
     */
    uint8_t graphics_font_color_1[4];

    /**
     * @brief Font color for level 2 (RGBA)
     */
    uint8_t graphics_font_color_2[4];

    /**
     * @brief Font size for level 1
     */
    uint8_t graphics_font_size_1;

    /**
     * @brief Font size for level 2
     */
    uint8_t graphics_font_size_2;


    /************* IMU *************/

    /**
     * @brief Device file name
     */
    char *imu_device;

    /**
     * @brief Gyroscope scale coefficient
     */
    float imu_gyro_scale;

    /**
     * @brief Gyroscope zero offset per axis (radian per second)
     */
    float imu_gyro_offset[3];

    /**
     * @brief Compass declination (radian)
     */
    float imu_mag_declination;

    /**
     * @brief Compass inclination (radian)
     */
    float imu_mag_inclination;

    /**
     * @brief Relative weight of the compass measurements
     */
    float imu_mag_weight;

    /**
     * @brief Relative weight of the accelerometer measurements
     */
    float imu_acc_weight;


    /************* GPS *************/

    /**
     * @brief Device file name
     */
    char *gps_device;
};

/**
 * @brief Application internal state
 */
typedef struct _application application_t;

/**
 * @brief Initializes the application
 * @param cfg Configuration structure
 * @return Pointer to application state or NULL on failure
 */
application_t *application_init(struct config *cfg);

/**
 * @brief Starts application main loop
 * @param app Internal state as returned by `application_init()`
 */
void application_mainloop(application_t *app);

/**
 * @brief Releases application resources
 * @param app Internal state as returned by `application_init()`
 */
void application_free(application_t *app);

#endif /* APPLICATION_H */
