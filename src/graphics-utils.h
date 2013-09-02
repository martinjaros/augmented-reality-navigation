/**
 * @file
 * @brief       Graphics internal utilities
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
 * This is an internal utility library for EGL/X11 initialization of the OpenGL ES 2.0 context.
 *
 */

#ifndef GRAPHICS_UTILS_H
#define GRAPHICS_UTILS_H

#include <stdint.h>
#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include <ft2build.h>
#include FT_FREETYPE_H

/* Atlas character mapping, using characters 32 - 255 of ISO-8859-1 / Unicode */
#define ATLAS_MAP_OFFSET      32
#define ATLAS_MAP_LENGTH      (256 - ATLAS_MAP_OFFSET)

/* Atlas texture size */
#define ATLAS_TEXTURE_WIDTH   512
#define ATLAS_TEXTURE_HEIGHT  512

//! @cond
struct _atlas
{
    GLuint texture;
    struct
    {
        float left, top, width, height;
        float tex_x, tex_y;
        float advance_x, advance_y;
    }
    chars[ATLAS_MAP_LENGTH];
};
//! @endcond

/**
 * @brief Compiles shader from source
 * @param type Shader type (GL_VERTEX_SHADER or GL_FRAGMENT_SHADER)
 * @param source Shader source code
 * @param length Source code length
 * @returns Shader ID
 */
GLuint shader_compile(GLenum type, const GLchar *source, GLint length);

/**
 * @brief Links compiled shaders into executable
 * @param vertex Vertex shader ID
 * @param fragment Fragment shader ID
 * @returns Program ID
 */
GLuint shader_link(GLuint vertex, GLuint fragment);

/**
 * @brief Creates EGL surface for native window
 * @param display EGL display to use
 * @param window Native window as returned by `window_create()`
 * @param[out] context Created context
 * @returns EGL surface
 */
EGLSurface surface_create(EGLDisplay display, EGLNativeWindowType window, EGLContext *context);

/**
 * @brief Load glyphs to font atlas
 * @param atlas Atlas to load into
 * @param font Font file path to use
 * @param size Font size
 * @return 1 on success, 0 on failure
 */
int atlas_load_glyphs(struct _atlas *atlas, const char *font, uint32_t size);

/**
 * @brief Creates geometry for given string
 * @param atlas Atlas object to use
 * @param[out] array Output geometry array
 * @param scale_x Horizontal scale of the drawing surface
 * @param scale_y Vertical scale of the drawing surface
 * @param text Input string
 * @return Number of elements written to array
 * @note Array should have at least 24*strlen(text) elements.
 */
GLuint atlas_get_geometry(struct _atlas *atlas, GLfloat *array, float scale_x, float scale_y, const char *text);

#endif /* GRAPHICS_UTILS_H */
