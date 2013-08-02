/**
 * @file
 * @brief       Application configuration
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
 * These are global definitions for configuring the application
 */

#ifndef CONFIG_H
#define CONFIG_H

/**
 * @brief Video device name (v4l2)
 */
#define VIDEODEV "/dev/video0"

/**
 * @brief GPS device name (tty)
 */
#define GPSDEV   "/dev/ttyS0"

/**
 * @brief IMU device name (i2c)
 */
#define IMUDEV   "/dev/i2c-0"

/**
 * @brief Waypoint database file name
 */
#define WPTF     "waypoints.txt"

/**
 * @brief Font file name
 */
#define FONTNAME "FreeSans.ttf"

/**
 * @brief Video frame width (pixels)
 */
#define VIDEO_WIDTH       800

/**
 * @brief Video frame height (pixels)
 */
#define VIDEO_HEIGHT      600

/**
 * @brief Video color format (fourcc)
 */
#define VIDEO_FORMAT      "RGB4"

/**
 * @brief Video interlacing (bool)
 */
#define VIDEO_INTERLACE   false

/**
 * @brief Video frame size (bytes)
 */
#define VIDEO_SIZE        4 * VIDEO_WIDTH * VIDEO_HEIGHT

/**
 * @brief Video frame horizontal field of view
 */
#define VIDEO_HFOV        60.0

/**
 * @brief Video frame vertical field of view
 */
#define VIDEO_VFOV        60.0

/**
 * @brief Font color
 */
#define FONT_COLOR        { 0, 0, 0, 255 }

/**
 * @brief Font size
 */
#define FONT_SIZE         12

#endif /* CONFIG_H */
