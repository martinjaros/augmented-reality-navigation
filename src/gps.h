/**
 * @file
 * @brief       GPS utilities
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
 * This is a utility library for GPS devices using NMEA 0183 protocol.
 * It works over serial tty line initializes by `gps_init()`, processing is done in separate thread.
 * @note All functions do not block
 *
 * Example:
 * @code
 * int main()
 * {
 *     gps_t *gps = gps_init("/dev/ttyS0");
 *
 *     while(1)
 *     {
 *         double lat, lon;
 *         float alt;
 *         gps_get_pos(gps, &lat, &lon, &alt);
 *
 *         // TODO: Do some processing here
 *
 *         sleep(1);
 *     }
 *
 *     gps_free(gps);
 * }
 * @endcode
 */

#ifndef GPS_H
#define GPS_H

/**
 * @brief Internal object
 */
typedef struct _gps gps_t;

/**
 * @brief Initializes GPS device
 * @param device Serial device name eg. "/dev/ttyS0"
 * @return GPS object or NULL on error
 */
gps_t *gps_init(const char *device);

/**
 * @brief Gets time information
 * @param gps Object returned by `gps_init()`
 * @param[out] time Time of day in seconds
 */
void gps_get_time(gps_t *gps, double *time);

/**
 * @brief Gets position information
 * @param gps Object returned by `gps_init()`
 * @param[out] lat Latitude in radians
 * @param[out] lon Longitude in radians
 * @param[out] alt Altitude in meters
 */
void gps_get_pos(gps_t *gps, double *lat, double *lon, float *alt);

/**
 * @brief Gets track information
 * @param gps Object returned by `gps_init()`
 * @param[out] speed Speed in km/h
 * @param[out] track Track angle in degrees
 */
void gps_get_track(gps_t *gps, float *speed, float *track);

/**
 * @brief Gets route information
 * @param gps Object returned by `gps_init()`
 * @param[out] waypoint Name of the waypoint (32 characters max)
 * @param[out] range Waypoint range in km
 * @param[out] bearing Bearing to waypoint in degrees
 */
void gps_get_route(gps_t *gps, char *waypoint, float *range, float *bearing);

/**
 * @brief Releases resources
 * @param gps Object returned by `gps_init()`
 */
void gps_free(gps_t *gps);

#endif /* GPS_H */
