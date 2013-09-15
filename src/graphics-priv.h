/**
 * @file
 * @brief       Graphics library - internal interfaces
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
 * This is a private header for graphics library.
 */

#ifndef GRAPHICS_PRIV_H
#error You are trying to include an internal graphics header. Please include `graphics.h`
#else

#include <EGL/egl.h>
#include <GLES2/gl2.h>

//! @cond

struct _graphics
{
    /* EGL surface and its size */
    EGLDisplay display;
    EGLSurface surface;
    EGLContext context;
    EGLint width, height;

    /* GLSL shader program and attributes */
    GLuint vert, frag, prog;
    GLint attr_coord, uni_offset, uni_scale, uni_rot, uni_tex, uni_color, uni_mask;
};

struct _drawable
{
    // Drawable type
    enum { DRAWABLE_LABEL, DRAWABLE_IMAGE } type;

    // Vertex buffer, its length and texture
    GLuint vbo, num, tex;

    // Colors
    GLfloat mask[4], color[4];
};

//! @endcond

#endif /* GRAPHICS_PRIV_H */
