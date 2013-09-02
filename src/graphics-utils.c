/*
 * Graphics internal utilities
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
#include <string.h>
#include <assert.h>

#include "debug.h"
#include "graphics-utils.h"

/* Helper macro, returns higher of the two arguments */
#define MAX(a,b) (((a)>(b))?(a):(b))

GLuint shader_compile(GLenum type, const GLchar *source, GLint length)
{
    DEBUG("shader_compile()");

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

GLuint shader_link(GLuint vertex, GLuint fragment)
{
    DEBUG("shader_link()");

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

EGLSurface surface_create(EGLDisplay display, EGLNativeWindowType window, EGLContext *context)
{
    DEBUG("surface_create()");
    assert(context != NULL);

    // Initialize EGL
    if(!eglInitialize(display, NULL, NULL) || !eglBindAPI(EGL_OPENGL_ES_API))
    {
        WARN("Failed to initialize EGL");
        return NULL;
    }

    // Configure EGL
    EGLConfig config;
    EGLint num, config_attr[5] = { EGL_SURFACE_TYPE, EGL_WINDOW_BIT, EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT, EGL_NONE };
    if(!eglChooseConfig(display, config_attr, &config, 1, &num) || (num != 1))
    {
        WARN("Failed to configure EGL");
        return NULL;
    }

    // Create window surface
    EGLSurface surface = eglCreateWindowSurface(display, config, window, NULL);
    if(!surface)
    {
        WARN("EGL failed to create window surface");
        return NULL;
    }

    // Create OpenGL context
    EGLint context_attr[] = { EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE };
    *context = eglCreateContext(display, config, NULL, context_attr);
    if(!*context)
    {
        WARN("EGL failed to create OpenGL context\n");
        eglDestroySurface(display, surface);
        return NULL;
    }

    // Use OpenGL context
    if(!eglMakeCurrent(display, surface, surface, *context))
    {
        WARN("EGL failed to use OpenGL context\n");
        eglDestroyContext(display, *context);
        eglDestroySurface(display, surface);
        return NULL;
    }

    return surface;
}

int atlas_load_glyphs(struct _atlas *atlas, const char *font, uint32_t size)
{
    DEBUG("atlas_load_glyphs()");
    assert(atlas != NULL);

    FT_Library ft;
    FT_Face face;
    if(FT_Init_FreeType(&ft))
    {
        WARN("Failed to init FreeType");
        return 0;
    }

    if(FT_New_Face(ft, font, 0, &face))
    {
        WARN("Failed to load font `%s`", font);
        goto error;
    }

    if(FT_Select_Charmap(face, FT_ENCODING_UNICODE))
    {
        WARN("Failed to select charmap");
        goto error;
    }

    if(FT_Set_Pixel_Sizes(face, 0, size))
    {
        WARN("Failed to set font size");
        goto error;
    }

    // Create texture
    glGenTextures(1, &atlas->texture);
    glBindTexture(GL_TEXTURE_2D, atlas->texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_ALPHA, ATLAS_TEXTURE_WIDTH, ATLAS_TEXTURE_HEIGHT, 0, GL_ALPHA, GL_UNSIGNED_BYTE, 0);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    int row_height = 0;
    int offset_x = 0;
    int offset_y = 0;

    int i;
    for(i = 0; i < ATLAS_MAP_LENGTH; i++)
    {
        if(FT_Load_Char(face, i + ATLAS_MAP_OFFSET, FT_LOAD_RENDER))
        {
            WARN("Failed to load character %d", i + ATLAS_MAP_OFFSET);
            return 0;
        }

        // Start new line if needed
        if(offset_x + face->glyph->bitmap.width + 1 >= ATLAS_TEXTURE_WIDTH)
        {
            offset_y += row_height;
            row_height = 0;
            offset_x = 0;

            if(offset_y + face->glyph->bitmap.rows + 1 >= ATLAS_TEXTURE_HEIGHT)
            {
                WARN("Atlas texture full at %d of %d characters", i, ATLAS_MAP_LENGTH);
                return 0;
            }
        }

        // Load glyph
        glTexSubImage2D(GL_TEXTURE_2D, 0, offset_x, offset_y, face->glyph->bitmap.width, face->glyph->bitmap.rows,
                        GL_ALPHA, GL_UNSIGNED_BYTE, face->glyph->bitmap.buffer);
        atlas->chars[i].advance_x = face->glyph->advance.x >> 6;
        atlas->chars[i].advance_y = face->glyph->advance.y >> 6;
        atlas->chars[i].width = face->glyph->bitmap.width;
        atlas->chars[i].height = face->glyph->bitmap.rows;
        atlas->chars[i].left = face->glyph->bitmap_left;
        atlas->chars[i].top = face->glyph->bitmap_top;
        atlas->chars[i].tex_x = offset_x / (float)ATLAS_TEXTURE_WIDTH;
        atlas->chars[i].tex_y = offset_y / (float)ATLAS_TEXTURE_HEIGHT;
        row_height = MAX(row_height, face->glyph->bitmap.rows);
        offset_x += face->glyph->bitmap.width + 1;
    }

    INFO("Atlas used %d rows of %d available", offset_y + face->glyph->bitmap.rows + 1, ATLAS_TEXTURE_WIDTH);
    FT_Done_FreeType(ft);
    return 1;

error:
    FT_Done_FreeType(ft);
    return 0;
}

GLuint atlas_get_geometry(struct _atlas *atlas, GLfloat *array, float scale_x, float scale_y, const char *text)
{
    DEBUG("atlas_get_geometry()");
    assert(atlas != NULL);
    assert(array != NULL);
    assert(text != NULL);

    int num = 0;
    float pos_x = 0;
    float pos_y = 0;

    uint8_t *pc = (uint8_t*)text;
    for(; *pc; pc++)
    {
        uint8_t c = *pc - ATLAS_MAP_OFFSET;
        if(c >= ATLAS_MAP_LENGTH) continue;
        float left = pos_x + atlas->chars[c].left * scale_x;
        float top = pos_y - atlas->chars[c].top * scale_y;
        float width = atlas->chars[c].width * scale_x;
        float height = atlas->chars[c].height * scale_y;

        pos_x += atlas->chars[c].advance_x * scale_x;
        pos_y += atlas->chars[c].advance_y * scale_y;
        if (!width || !height) continue;

        // Left bottom
        array[num + 0] = left;
        array[num + 1] = -top;
        array[num + 2] = atlas->chars[c].tex_x;
        array[num + 3] = atlas->chars[c].tex_y;
        num += 4;

        // Right bottom
        array[num + 0] = left + width;
        array[num + 1] = -top;
        array[num + 2] = atlas->chars[c].tex_x + atlas->chars[c].width / ATLAS_TEXTURE_WIDTH;
        array[num + 3] = atlas->chars[c].tex_y;
        num += 4;

        // Left top
        array[num + 0] = left;
        array[num + 1] = -top - height;
        array[num + 2] = atlas->chars[c].tex_x;
        array[num + 3] = atlas->chars[c].tex_y + atlas->chars[c].height / ATLAS_TEXTURE_HEIGHT;
        num += 4;

        // Right bottom
        array[num + 0] = left + width;
        array[num + 1] = -top;
        array[num + 2] = atlas->chars[c].tex_x + atlas->chars[c].width / ATLAS_TEXTURE_WIDTH;
        array[num + 3] = atlas->chars[c].tex_y;
        num += 4;

        // Left top
        array[num + 0] = left;
        array[num + 1] = -top - height;
        array[num + 2] = atlas->chars[c].tex_x;
        array[num + 3] = atlas->chars[c].tex_y + atlas->chars[c].height / ATLAS_TEXTURE_HEIGHT;
        num += 4;

        // Right top
        array[num + 0] = left + width;
        array[num + 1] = -top - height;
        array[num + 2] = atlas->chars[c].tex_x + atlas->chars[c].width / ATLAS_TEXTURE_WIDTH;
        array[num + 3] = atlas->chars[c].tex_y + atlas->chars[c].height / ATLAS_TEXTURE_HEIGHT;
        num += 4;
    }

    return num;
}
