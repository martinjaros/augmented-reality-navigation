/**
 * @file
 * @brief       GPS - configuration
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
 * These are configuration definitions for GPS.
 */

#ifndef GPS_CONFIG_H
#define GPS_CONFIG_H

/**
 * @brief GPS configuration structure
 */
struct gps_config
{
    /**
     * @brief Digital elevation model file name
     */
    char *dem_file;

    /**
     * @brief Digital elevation model left border in radians
     */
    double dem_left;

    /**
     * @brief Digital elevation model top border in radians
     */
    double dem_top;

    /**
     * @brief Digital elevation model right border in radians
     */
    double dem_right;

    /**
     * @brief Digital elevation model bottom border in radians
     */
    double dem_bottom;

    /**
     * @brief Digital elevation model pixel scale in meters
     */
    float dem_pixel_scale;

    /**
     * @brief Landmark database file name
     */
    char *datafile;

    /**
     * @brief User specified data for `create_label()` callback
     */
    void *userdata;

    /**
     * @brief Callback function for synchronous creation of drawable label
     */
    void*(*create_label)(const char *text, void *userdata);

    /**
     * @brief Callback function for synchronous deletion of drawable label
     */
    void(*delete_label)(void *label);
};

#endif /* GPS_CONFIG_H */
