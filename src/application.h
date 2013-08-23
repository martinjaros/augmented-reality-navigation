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

#include "config.h"

/**
 * @brief Application configuration structure
 */
struct config
{
    /**
     * @brief Video device name
     */
    char *videodev;

    /**
     * @brief GPS device name
     */
    char *gpsdev;

    /**
     * @brief IMU device name
     */
    char *imudev;

    /**
     * @brief Waypoint database file name
     */
    char *wptf;

    /**
     * @brief Font file name
     */
    char *fontname;

    /**
     * @brief Calibration file name
     */
    char *calibname;
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
