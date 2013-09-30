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
 *     graphics_t *g = graphics_init(0);
 *     atlas_t *atlas = graphics_atlas_create("FreeSans.ttf", 20);
 *
 *     drawable_t *label = graphics_label_create(g, atlas);
 *     graphics_label_set_text(label, 0, "Hello World");
 *
 *     for(;;)
 *     {
 *         graphics_draw(g, label, 10, 10, 1, 0);
 *         graphics_flush(g, "\x00\x00\x00");
 *     }
 *
 *     graphics_drawable_free(label);
 *     atlas_free(atlas);
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
 * @brief Drawable object
 */
typedef struct _drawable drawable_t;

/**
 * @brief Atlas of characters
 */
typedef struct _atlas atlas_t;

/**
 * @brief HUD composite object
 */
typedef struct _hud hud_t;

/**
 * @brief Anchor options
 */
enum anchor_types
{
    ANCHOR_LEFT_BOTTOM = 0,
    ANCHOR_LEFT_TOP,
    ANCHOR_CENTER_TOP,
    ANCHOR_RIGHT_TOP,
    ANCHOR_RIGHT_BOTTOM,
    ANCHOR_CENTER_BOTTOM,
    ANCHOR_CENTER
};

/**
 * @brief Initializes graphics
 * @param window native window id
 * @return Internal graphics object
 */
graphics_t *graphics_init(uint32_t window);

/**
 * @brief Creates font atlas
 * @param font TTF font file to use
 * @param size Font size
 * @return Atlas object
 */
atlas_t *graphics_atlas_create(const char *font, uint32_t size);

/**
 * @brief Swaps framebuffers
 * @param g Internal graphics object as returned by `graphics_init()`
 * @param color If not NULL, this is a RBG color used to clear the screen
 * @return 1 on success, 0 on failure
 * @note Passing NULL as color will skip clearing the framebuffer.
 */
int graphics_flush(graphics_t *g, const uint8_t *color);

/**
 * @brief Draws object
 * @param g Internal graphics object as returned by `graphics_init()`
 * @param d Drawable object to draw
 * @param x Horizontal coordinate
 * @param y Vertical coordinate
 * @param scale Relative scale (0-1)
 * @param rotation Rotation angle in radians
 */
void graphics_draw(graphics_t *g, drawable_t *d, int x, int y, float scale, float rotation);

/**
 * @brief Creates drawable label
 * @param g Internal graphics object as returned by `graphics_init()`
 * @param atlas Atlas object as returned by `graphics_atlas_create()`
 * @param anchor Anchor used for drawing
 * @return Drawable object
 * @note Label internally holds pointer to the given atlas
 */
drawable_t *graphics_label_create(graphics_t *g, atlas_t *atlas, enum anchor_types anchor);

/**
 * @brief Creates drawable image
 * @param g Internal graphics object as returned by `graphics_init()`
 * @param width Texture width in pixels
 * @param height Texture height in pixels
 * @param anchor Anchor used for drawing
 * @return Drawable object
 */
drawable_t *graphics_image_create(graphics_t *g, uint32_t width, uint32_t height, enum anchor_types anchor);

/**
 * @brief Creates HUD overlay
 * @param g Internal graphics object as returned by `graphics_init()`
 * @param atlas Atlas object as returned by `graphics_atlas_create()`
 * @param color RGBA color
 * @param font_size Size of font
 * @param hfov Horizontal field of view in radians
 * @param vfov Vertical field of view in radians
 * @return HUD object
 */
hud_t *graphics_hud_create(graphics_t *g, atlas_t *atlas, uint8_t color[4], uint32_t font_size, float hfov, float vfov);

/**
 * @brief Draws HUD overlay
 * @param hud HUD object
 * @param attitude Roll, bank and yaw angles in radians
 * @param speed Speed in km/h
 * @param altitude Altitude in meters
 * @param track Track angle in radians
 * @param bearing Bearing to waypoint in radians
 * @param distance Distance to waypoint in kilometers
 * @param waypoint Waypoint name shown centered on top
 */
void graphics_hud_draw(hud_t *hud, float attitude[3], float speed, float altitude, float track, float bearing, float distance, const char *waypoint);

/**
 * @brief Releases resources from HUD object
 * @param hud HUD object
 */
void graphics_hud_free(hud_t *hud);

/**
 * @brief Updates label text
 * @param label Label object to update
 * @param text NULL terminated string
 */
void graphics_label_set_text(drawable_t *label, const char *text);

/**
 * @brief Updates label color
 * @param label Label object to update
 * @param color Font color in RGBA format
 */
void graphics_label_set_color(drawable_t *label, const uint8_t color[4]);

/**
 * @brief Updates image bitmap
 * @param image Image object to update
 * @param buffer Pixel data buffer in RGBx format (width * height * 32)
 */
void graphics_image_set_bitmap(drawable_t *image, const void *buffer);

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
 * @param g Internal graphics object as returned by `graphics_init()`
 * @note Does not release nested objects, use `graphics_atlas_free()` and `graphics_drawable_free()`.
 */
void graphics_free(graphics_t *g);

#endif /* GRAPHICS_H */
