/*
 * Graphics library - core utilities
 *
 * Copyright (C) 2013 - Martin Jaros <xjaros32@stud.feec.vutbr.cz>
 *
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
 */

#include <stdlib.h>
#include <assert.h>
#include <string.h>

#include "debug.h"
#include "graphics.h"

#define GRAPHICS_PRIV_H
#include "graphics-priv.h"

#define SHADER_VERTEX_SRC \
"attribute vec4 coord;\n" \
"uniform vec2 offset;\n" \
"uniform vec2 scale;\n" \
"uniform float rot;\n" \
"varying vec2 texpos;\n" \
"void main()\n" \
"{\n" \
"  float sinrot = sin(rot);\n" \
"  float cosrot = cos(rot);\n" \
"  gl_Position = vec4(vec2(coord.x * cosrot - coord.y * sinrot, coord.x * sinrot + coord.y * cosrot) * scale + offset, 0, 1);\n" \
"  texpos = coord.zw;\n" \
"}\n"

#define SHADER_FRAGMENT_SRC \
"uniform mediump vec4 color;\n" \
"uniform mediump vec4 mask;\n" \
"uniform sampler2D tex;\n" \
"varying lowp vec2 texpos;\n" \
"void main()\n" \
"{\n" \
"  gl_FragColor = texture2D(tex, texpos) * mask + color;\n" \
"}\n"

static GLuint shader_compile(GLenum type, const GLchar *source, GLint length)
{
    // Compile shader
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, &length);
    glCompileShader(shader);

    // Get compilation status
    GLint status;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
    if(!status)
    {
        int len;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &len);

        char* str = malloc(len);
        glGetShaderInfoLog(shader, len, NULL, str);
        WARN("Shader compiler error:\n%s", str);
        free(str);

        return 0;
    }

    return shader;
}

static GLuint shader_link(GLuint vertex, GLuint fragment)
{
    // Attach and link shaders
    GLuint program = glCreateProgram();
    glAttachShader(program, vertex);
    glAttachShader(program, fragment);
    glLinkProgram(program);

    // Get link status
    GLint status;
    glGetProgramiv(program, GL_LINK_STATUS, &status);
    if(!status)
    {
        int len;
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &len);

        char* str = malloc(len);
        glGetProgramInfoLog(program, len, NULL, str);
        WARN("Shader linker error:%s", str);
        free(str);

        return 0;
    }

    return program;
}

graphics_t *graphics_init(uint32_t window)
{
    DEBUG("graphics_init()");

    graphics_t *g = calloc(1, sizeof(struct _graphics));
    assert(g != 0);

    // Get display
    if((g->display = eglGetDisplay(EGL_DEFAULT_DISPLAY)) == NULL)
    {
        WARN("Failed to get display");
        goto error;
    }

    // Initialize EGL
    if(!eglInitialize(g->display, NULL, NULL) || !eglBindAPI(EGL_OPENGL_ES_API))
    {
        WARN("Failed to initialize EGL");
        goto error;
    }

    // Configure EGL
    EGLConfig config;
    EGLint num, config_attr[5] = { EGL_SURFACE_TYPE, EGL_WINDOW_BIT, EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT, EGL_NONE };
    if(!eglChooseConfig(g->display, config_attr, &config, 1, &num) || (num != 1))
    {
        WARN("Failed to configure EGL");
        goto error;
    }

    // Create window surface
    if((g->surface = eglCreateWindowSurface(g->display, config, window, NULL)) == NULL)
    {
        WARN("EGL failed to create window surface");
        goto error;
    }

    // Create OpenGL context
    EGLint context_attr[] = { EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE };
    if((g->context = eglCreateContext(g->display, config, NULL, context_attr)) == NULL)
    {
        WARN("EGL failed to create OpenGL context\n");
        goto error;
    }

    // Use OpenGL context
    if(!eglMakeCurrent(g->display, g->surface, g->surface, g->context))
    {
        WARN("EGL failed to use OpenGL context\n");
        goto error;
    }

    // Compile shader
    static const GLchar shader_vert[] = SHADER_VERTEX_SRC;
    static const GLchar shader_frag[] = SHADER_FRAGMENT_SRC;
    if(!(g->vert = shader_compile(GL_VERTEX_SHADER, shader_vert, sizeof(shader_vert))) ||
       !(g->frag = shader_compile(GL_FRAGMENT_SHADER, shader_frag, sizeof(shader_frag))) ||
       !(g->prog = shader_link(g->vert, g->frag)))
    {
        WARN("Cannot compile shader");
        goto error;
    }

    // Get shader attributes
    glUseProgram(g->prog);
    g->uni_tex = glGetUniformLocation(g->prog, "tex");  // Texture
    g->uni_color = glGetUniformLocation(g->prog, "color"); // RGBA drawing color
    g->uni_mask = glGetUniformLocation(g->prog, "mask"); // RGBA color mask
    g->uni_offset = glGetUniformLocation(g->prog, "offset"); // X, Y drawing coordinates
    g->uni_scale = glGetUniformLocation(g->prog, "scale"); // Scale coefficient
    g->uni_rot = glGetUniformLocation(g->prog, "rot"); // Rotation angle [rad]
    g->attr_coord = glGetAttribLocation(g->prog, "coord"); // Model vertex coordinates
    if((g->uni_tex == -1) || (g->uni_color == -1) || (g->uni_mask == -1) || (g->uni_offset == -1) || (g->uni_scale == -1) || (g->uni_rot == -1) || (g->attr_coord == -1))
    {
        WARN("Failed to get attribute locations");
        goto error;
    }
    glEnableVertexAttribArray(g->attr_coord);

    // Enable blending
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // Get surface dimensions
    if(!eglQuerySurface(g->display, g->surface, EGL_WIDTH, &g->width) ||
       !eglQuerySurface(g->display, g->surface, EGL_HEIGHT, &g->height))
    {
        WARN("Failed to query surface");
        goto error;
    }

    // Check OpenGL errors
    int err = glGetError();
    if(err != GL_NO_ERROR)
    {
        WARN("OpenGL error %d", err);
        goto error;
    }

    return g;

error:
   if(g->prog) glDeleteProgram(g->prog);
   if(g->vert) glDeleteShader(g->vert);
   if(g->frag) glDeleteShader(g->frag);
   if(g->display) eglMakeCurrent(g->display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
   if(g->context) eglDestroyContext(g->display, g->context);
   if(g->surface) eglDestroySurface(g->display, g->surface);
   free(g);
   return NULL;
}

int graphics_flush(graphics_t *g, const uint8_t *color)
{
    DEBUG("graphics_flush()");
    assert(g != 0);

    if(!eglSwapBuffers(g->display, g->surface)) return 0;
    if(color)
    {
        glClearColor(color[0] / 255.0, color[1] / 255.0, color[2] / 255.0, 0);
        glClear(GL_COLOR_BUFFER_BIT);
    }

    // Check OpenGL errors
    int err = glGetError();
    if(err != GL_NO_ERROR)
    {
        WARN("OpenGL error %d", err);
        return 0;
    }

    return 1;
}

void graphics_free(graphics_t *g)
{
    DEBUG("graphics_free()");
    assert(g != 0);

    glDeleteProgram(g->prog);
    glDeleteShader(g->vert);
    glDeleteShader(g->frag);
    eglMakeCurrent(g->display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    eglDestroyContext(g->display, g->context);
    eglDestroySurface(g->display, g->surface);
    free(g);
}
