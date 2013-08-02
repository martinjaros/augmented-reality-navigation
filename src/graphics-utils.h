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

#include <EGL/egl.h>
#include <GLES2/gl2.h>

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
 * @brief Creates X11 fullscreen window
 * @param display Pointer where to output opened display
 * @returns Window XID
 * @note If X11 support is not enabled, returns 0
 */
EGLNativeWindowType window_create(EGLNativeDisplayType *display);

/**
 * @brief Destroys X11 window and closes display
 * @param display X11 display to close
 * @param window X11 window XID to destroy
 * @note If X11 support is not enabled, does nothing
 */
void window_destroy(EGLNativeDisplayType display, EGLNativeWindowType window);

/**
 * @brief Creates EGL surface for native window
 * @param display EGL display to use
 * @param window Native window as returned by `window_create()`
 * @param context Pointer where to output created context
 * @returns EGL surface
 */
EGLSurface surface_create(EGLDisplay display, EGLNativeWindowType window, EGLContext *context);

#endif /* GRAPHICS_UTILS_H */
