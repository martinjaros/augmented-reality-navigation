/**
 * @file
 * @brief       GPS internal utilities
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
 * Digital elevation model and waypoint handling utilities for GPS subsystem
 */

#ifndef GPS_UTIL_H
#define GPS_UTIL_H

#include <stdint.h>

struct dem
{
    uint8_t **lines;
    uint32_t width, height;
    double left, right, top, bottom;
    float pixel_scale;
};

struct waypoint_node
{
    double lat, lon;
    float alt;
    char name[32];
    void *label;
    struct waypoint_node *next;
};

struct waypoint_node *gps_util_load_datafile(const char *filename, struct dem *dem);

struct dem *gps_util_load_demfile(const char *filename, double left, double top, double right, double bottom, float scale);

float gps_util_dem_get_alt(struct dem *dem, double lat, double lon);

#endif /* GPS_UTIL_H */

