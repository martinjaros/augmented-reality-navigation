/**
 * @file
 * @brief       Navigation utilities
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
 * This is utility library for navigation calculations. `nav_load()` loads
 * waypoint list from a file, you can iterate over the waypoints and calculate
 * projection angles for each.
 *
 * Example:
 * @code
 * int main()
 * {
 *     struct attdinfo attd;
 *     struct posinfo pos;
 *     struct wptinfo wpt;
 *
 *     wpt_iter *iter = nav_load("waypoints.txt");
 *
 *     while(1)
 *     {
 *         // TODO: Read attitude and position from devices
 *
 *         nav_reset(iter);
 *
 *         while(nav_iter(iter, &attd, &pos, &wpt))
 *         {
 *             // TODO: Process waypoint
 *         }
 *     }
 *
 *     nav_free(iter);
 * }
 * @endcode
 */

#ifndef NAV_H
#define NAV_H

#include "gps.h"
#include "imu.h"
#include "graphics.h"

/**
 * @brief Waypoint iterator
 */
typedef struct _wpt_iter wpt_iter;

/**
 * @brief Waypoint information
 */
struct wptinfo
{
    /**
     * @brief Label to display
     */
    drawable_t *label;

    /**
     * @brief Horizontal projection angle in radians
     */
    double x;

    /**
     * @brief Vertical projection angle in radians
     */
    double y;
};

/**
 * @brief Loads waypoint list from file and creates iterator
 * @param filename Name of the file to load eg. "waypoints.txt"
 * @param g Graphics state used for loading characters
 * @param atlas Character atlas
 * @param color Label color (RGBA)
 * @return Iterator for the loaded waypoints or NULL on error
 */
wpt_iter *nav_load(const char *filename, graphics_t *g, atlas_t *atlas, uint8_t color[4]);

/**
 * @brief Resets the iterator to start from begin
 * @param iter Iterator as returned by `nav_load()`
 */
void nav_reset(wpt_iter *iter);

/**
 * @brief Iterates over waypoints, calculating their projections
 * @param iter Iterator as returned by `nav_load()`
 * @param attd Attitude information to use for projection
 * @param pos Position information to use for projection
 * @param[out] wpt Structure where to output results
 * @return 0 if there are no more waypoints to iterate, 1 otherwise
 */
int nav_iter(wpt_iter *iter, const struct attdinfo *attd, const struct posinfo *pos, struct wptinfo *wpt);

/**
 * @brief Frees allocated resources by the iterator
 * @param iter Iterator as returned by `nav_load()`
 */
void nav_free(wpt_iter *iter);

#endif /* NAV_H */
