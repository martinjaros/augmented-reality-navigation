/**
 * @file
 * @brief       Graphics utilities
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
 * This is an interface for graphics rendering using OpenGL ES 2.0 acceleration.
 * To enable X11 binding define X11BUILD symbol.
 *
 * Example:
 * @code
 * int main()
 * {
 *     graphics_t *g = graphics_init("FreeSans.ttf", 24);
 *
 *     uint8_t white[] = { 255, 255, 255, 255 };
 *     uint8_t black[] = { 0, 0, 0, 255 };
 *
 *     while(1)
 *     {
 *         graphics_clear(g, white);
 *         graphics_draw_text(g, "Hello World", 0, 24, black);
 *         if(!graphics_flush(g)) break;
 *     }
 *
 *     graphics_free(g);
 *     return 0;
 * }
 * @endcode
 */

#ifndef GRAPHICS_H
#define GRAPHICS_H

#include <stdint.h>

/**
 * @brief Graphics internal state
 */
typedef struct _graphics graphics_t;

/**
 * @brief Initializes OpenGL and loads FreeType font
 * @param font_name Path to font file to load
 * @param font_height Size of the font
 * @return NULL on error
 */
graphics_t *graphics_init(const char *font_name, int font_height);

/**
 * @brief Returns drawing surface dimension
 * @param g Internal state as returned by `graphics_init()`
 * @param[out] width Pointer where to place surface width in pixels
 * @param[out] height Pointer where to place surface height in pixels
 */
void graphics_get_dimensions(graphics_t *g, uint32_t *width, uint32_t *height);

/**
 * @brief Clears a surface with specified color
 * @param g Internal state as returned by `graphics_init()`
 * @param color RGBA color used as background
 */
void graphics_clear(graphics_t *g, const uint8_t color[4]);

/**
 * @brief Draws the image at the specified location
 * @param g Internal state as returned by `graphics_init()`
 * @param img Pointer to the image pixel data
 * @param width Image width in pixels
 * @param height Image height in pixels
 * @param x Horizontal coordinate
 * @param y Vertical coordinate
 * @note Image uses RGBA32 format
 */
void graphics_draw_image(graphics_t *g, const void *img, uint32_t width, uint32_t height, uint32_t x, uint32_t y);

/**
 * @brief Draws text at the specified location
 * @param g Internal state as returned by `graphics_init()`
 * @param str NULL terminated string to draw
 * @param color RGBA color to use for drawing
 * @param x Horizontal coordinate
 * @param y Vertical coordinate
 */
void graphics_draw_text(graphics_t *g, const char *str, uint8_t color[4], uint32_t x, uint32_t y);

/**
 * @brief Flushes framebuffer to the display
 * @param g Internal state as returned by `graphics_init()`
 * @return 0 if cannot display the framebuffer, 1 otherwise
 */
int graphics_flush(graphics_t *g);

/**
 * @brief Frees all allocated resources
 * @param g Internal state as returned by `graphics_init()`
 */
void graphics_free(graphics_t *g);

#endif /* GRAPHICS_H */
