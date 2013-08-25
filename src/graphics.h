/**
 * @file
 * @brief       Graphics library
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
 * This is OpenGL ES 2.0 graphics library.
 *
 * Example:
 * @code
 * int main ()
 * {
 *     graphics_t *g = graphics_init();
 *     atlas_t *atlas = graphics_atlas_create("FreeSans.ttf", 20);
 *
 *     drawable_t *label = graphics_label_create(g);
 *     graphics_label_set_text(g, label, atlas, "Hello World");
 *
 *     for(;;)
 *     {
 *         graphics_draw(g, label, 10, 10);
 *         graphics_flush(g, NULL);
 *     }
 *
 *     graphics_drawable_free(label);
 *     graphics_atlas_free(atlas);
 *     graphics_free(g);
 *
 *     return(0);
 * }
 * @endcode
 */

#ifndef GRAPHICS_H
#define GRAPHICS_H

#include <stdint.h>

/**
 * @brief Internal graphics state
 */
typedef struct _graphics graphics_t;

/**
 * @brief Atlas of characters
 */
typedef struct _atlas atlas_t;

/**
 * @brief Drawable object
 */
typedef struct _drawable drawable_t;

/**
 * @brief Initializes graphics
 * @param width Requested surface width
 * @param height Requested surface height
 * @return Internal graphics state
 */
graphics_t *graphics_init(uint16_t width, uint16_t height);

/**
 * @brief Flushes framebuffer
 * @param g Internal graphics state as returned by `graphics_init()`
 * @param color If not NULL, this is a RBGA color used to clear the screen
 * @return 1 on success, 0 on failure
 */
int graphics_flush(graphics_t *g, const uint8_t *color);

/**
 * @brief Draws verticies
 * @param g Internal graphics state as returned by `graphics_init()`
 * @param d Drawable object
 * @param x Horizontal coordinate
 * @param y Vertical coordinate
 */
void graphics_draw(graphics_t *g, drawable_t *d, uint32_t x, uint32_t y);

/**
 * @brief Creates font atlas
 * @param font Font file path to use
 * @param size Font size
 * @return Atlas structure
 */
atlas_t *graphics_atlas_create(const char *font, uint32_t size);

/**
 * @brief Creates drawable label
 * @param g Internal graphics state as returned by `graphics_init()`
 * @return Drawable object
 */
drawable_t *graphics_label_create(graphics_t *g);

/**
 * @brief Creates drawable image
 * @param g Internal graphics state as returned by `graphics_init()`
 * @param width Texture width
 * @param height Texture height
 * @return Drawable object
 */
drawable_t *graphics_image_create(graphics_t *g, uint32_t width, uint32_t height);

/**
 * @brief Updates label text
 * @param g Internal graphics state as returned by `graphics_init()`
 * @param label Label drawable to update
 * @param atlas Character atlas
 * @param text NULL terminated string to use
 */
void graphics_label_set_text(graphics_t *g, drawable_t *label, atlas_t *atlas, const char *text);


/**
 * @brief Updates label color
 * @param g Internal graphics state as returned by `graphics_init()`
 * @param label Label drawable to update
 * @param color Font color in RGBA format
 */
void graphics_label_set_color(graphics_t *g, drawable_t *label, uint8_t color[4]);

/**
 * @brief Updates image bitmap
 * @param g Internal graphics state as returned by `graphics_init()`
 * @param image Image drawable to update
 * @param buffer Pixel data buffer in RGBx format (width * height * 32)
 */
void graphics_image_set_bitmap(graphics_t *g, drawable_t *image, const void *buffer);

/**
 * @brief Releases resources of the specified object
 * @param d Drawable object to free
 */
void graphics_drawable_free(drawable_t *d);

/**
 * @brief Releases resources of the specified atlas
 * @param atlas Atlas object to free
 */
void graphics_atlas_free(atlas_t *atlas);

/**
 * @brief Releases graphics resources
 * @param g Internal graphics state as returned by `graphics_init()`
 * @note Does not release nested objects, use `graphics_atlas_free()` and `graphics_drawable_free()`
 */
void graphics_free(graphics_t *g);

#endif /* GRAPHICS_H */
