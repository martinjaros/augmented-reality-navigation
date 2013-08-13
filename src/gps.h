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
 * It works over serial tty line opened by `gps_open`, which returns a file descriptor
 * to be used for `select()` or `poll()`. Positional data could be then retrieved by `gps_read()`.
 * @note All functions do not block
 *
 * Example:
 * @code
 * int main()
 * {
 *     int fd = gps_open("/dev/ttyS0");
 *
 *     while(1)
 *     {
 *         fd_set fds;
 *         FD_ZERO(&fds);
 *         FD_SET(fd, &fds);
 *
 *         select(fd + 1, &fds, NULL, NULL, NULL);
 *
 *         struct posinfo pos;
 *         if(gps_read(fd, &pos))
 *         {
 *             // TODO: Do some processing here
 *         }
 *     }
 *
 *     gps_close(fd);
 * }
 * @endcode
 */

#ifndef GPS_H
#define GPS_H

/**
 * @brief Position, track and destination information
 */
struct gpsdata
{
    /**
     * @brief Time in seconds
     */
    double time;

    /**
     * @brief Latitude in radians
     */
    double lat;

    /**
     * @brief Longitude in radians
     */
    double lon;

    /**
     * @brief Altitude in meters
     */
    double alt;

    /**
     * @brief Speed in kilometers per hour
     */
    double spd;

    /**
     * @brief Track angle in degrees
     */
    double trk;

    /**
     * @brief Cross track error in kilometers (right positive, left negative)
     */
    double xte;

    /**
     * @brief Origin waypoint name
     */
    char orig_name[32];

    /**
     * @brief Destination waypoint name
     */
    char dest_name[32];

    /**
     * @brief Destination waypoint latitude in radians
     */
    double dest_lat;

    /**
     * @brief Destination waypoint longitude in radians
     */
    double dest_lon;

    /**
     * @brief Destination waypoint range in kilometers
     */
    double dest_range;

    /**
     * @brief Destination waypoint bearing in degrees
     */
    double dest_bearing;

    /**
     * @brief Destination waypoint closing velocity in kilometers per hour
     */
    double dest_velocity;
};

/**
 * @brief Opens GPS device and initializes tty line
 * @param devname Serial device name eg. "/dev/ttyS0"
 * @return File descriptor or -1 on error
 */
int gps_open(const char *devname);

/**
 * @brief Reads and parses NMEA 0183 sentence
 * @param fd File descriptor as returned by `gps_open()`
 * @param[out] data Pointer to structure where to output results
 * @return 1 if the structure was modified, 0 otherwise
 */
int gps_read(int fd, struct gpsdata *data);

/**
 * @brief Closes the device
 * @param fd File descriptor as returned by `gps_open()`
 */
void gps_close(int fd);

#endif /* GPS_H */
