/*
 * Graphics library
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

#include <ft2build.h>
#include FT_FREETYPE_H

#include "graphics.h"
#include "graphics-utils.h"

#include "shaders.inc"

/* Helper macro, returns higher of the two arguments */
#define MAX(a,b) (((a)>(b))?(a):(b))

struct _graphics
{
    /* Native types*/
    EGLNativeDisplayType native_display;
    EGLNativeWindowType native_window;

    /* EGL surface and its size */
    EGLDisplay display;
    EGLSurface surface;
    EGLContext context;
    EGLint width, height;

    /* GLSL shader program and attributes */
    GLuint vert, frag, prog;
    GLint attr_coord, uni_offset, uni_tex, uni_color, uni_mask;
};

struct _atlas
{
    GLuint texture, width, height;
    struct
    {
        float left, top, width, height;
        float tex_x, tex_y;
        float advance_x, advance_y;
    }
    chars[96];
};

struct _drawable
{
    // Drawable type
    enum { DRAWABLE_LABEL, DRAWABLE_IMAGE } type;

    // Vertex buffer
    uint32_t vbo;

    // Vertex counter
    uint32_t num;

    // Texture
    uint32_t tex;

    // Colors
    GLfloat mask[4], color[4];
};

graphics_t *graphics_init()
{
    graphics_t *g = calloc(1, sizeof(graphics_t));

    /* Create window */
    g->native_window = window_create(&g->native_display);
    g->display = eglGetDisplay(g->native_display);

    /* Create surface */
    if(!(g->surface = surface_create(g->display, g->native_window, &g->context)))
    {
        window_destroy(g->native_display, g->native_window);
        free(g);
        return NULL;
    }

    /* Compile shader */
    if(!(g->vert = shader_compile(GL_VERTEX_SHADER, (GLchar*)src_shader_vert, src_shader_vert_len)) ||
       !(g->frag = shader_compile(GL_FRAGMENT_SHADER, (GLchar*)src_shader_frag, src_shader_frag_len)) ||
       !(g->prog = shader_link(g->vert, g->frag)))
    {
        eglMakeCurrent(g->display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        eglDestroyContext(g->display, g->context);
        eglDestroySurface(g->display, g->surface);
        window_destroy(g->native_display, g->native_window);
        free(g);
        return NULL;
    }

    /* Get shader attributes */
    glUseProgram(g->prog);
    g->uni_tex = glGetUniformLocation(g->prog, "tex");  // Texture
    g->uni_color = glGetUniformLocation(g->prog, "color"); // RGBA drawing color
    g->uni_mask = glGetUniformLocation(g->prog, "mask"); // RGBA color mask
    g->uni_offset = glGetUniformLocation(g->prog, "offset"); // X, Y drawing coordinates
    g->attr_coord = glGetAttribLocation(g->prog, "coord"); // Model vertex coordinates
    glEnableVertexAttribArray(g->attr_coord);

    /* Enable blending */
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    /* Get surface dimensions */
    eglQuerySurface(g->display, g->surface, EGL_WIDTH, &g->width);
    eglQuerySurface(g->display, g->surface, EGL_HEIGHT, &g->height);

    return g;
}

int graphics_flush(graphics_t *g, const uint8_t *color)
{
    assert(g != NULL);

    int res = eglSwapBuffers(g->display, g->surface);

    if(color)
    {
        glClearColor(color[0] / 255.0, color[1] / 255.0, color[2] / 255.0, color[3] / 255.0);
        glClear(GL_COLOR_BUFFER_BIT);
    }

    return res;
}

void graphics_draw(graphics_t *g, drawable_t *d, uint32_t x, uint32_t y)
{
    assert(g != NULL);
    assert(d != NULL);

    if(d->num == 0) return;

    // Set position
    GLfloat pos[] = { (GLfloat)x * 2.0 / (GLfloat)g->width - 1.0, (GLfloat)y * -2.0 / (GLfloat)g->height + 1.0 };
    glUniform2fv(g->uni_offset, 1, pos);

    // Set colors
    glUniform4fv(g->uni_mask, 1, d->mask);
    glUniform4fv(g->uni_color, 1, d->color);

    // Bind the texture
    glBindTexture(GL_TEXTURE_2D, d->tex);
    glUniform1i(g->uni_tex, 0);

    // Bind the vertex buffer
    glBindBuffer(GL_ARRAY_BUFFER, d->vbo);
    glVertexAttribPointer(g->attr_coord, 4, GL_FLOAT, GL_FALSE, 0, 0);

    // Draw arrays
    glDrawArrays(GL_TRIANGLES, 0, d->num);
}

atlas_t *graphics_atlas_create(const char *font, uint32_t size)
{
    int i;

    assert(font != NULL);
    assert(size > 0);

    atlas_t *atlas = malloc(sizeof(struct _atlas));
    assert(atlas != NULL);

    FT_Library ft;
    FT_Face face;
    if(FT_Init_FreeType(&ft) || FT_New_Face(ft, font, 0, &face)) return NULL;
    FT_Set_Pixel_Sizes(face, 0, size);

    FT_GlyphSlot glyph = face->glyph;
    int row_width = 0, row_height = 0;

    // Calculate texture size
    atlas->width = 0;
    atlas->height = 0;
    for(i = 0; i < 96; i++)
    {
        if(FT_Load_Char(face, i + 32, FT_LOAD_RENDER)) continue;

        // Start new line if needed
        if(row_width + glyph->bitmap.width + 1 >= 1024)
        {
            atlas->width = MAX(atlas->width, row_width);
            atlas->height += row_height;
            row_width = 0;
            row_height = 0;
        }

        row_width += glyph->bitmap.width + 1;
        row_height = MAX(row_height, glyph->bitmap.rows);
    }
    atlas->width = MAX(atlas->width, row_width);
    atlas->height += row_height;

    // Create texture
    glGenTextures(1, &atlas->texture);
    glBindTexture(GL_TEXTURE_2D, atlas->texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_ALPHA, atlas->width, atlas->height, 0, GL_ALPHA, GL_UNSIGNED_BYTE, 0);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    int offset_x = 0;
    int offset_y = 0;

    // Load characters
    row_height = 0;
    for(i = 0; i < 96; i++)
    {
        if (FT_Load_Char(face, i + 32, FT_LOAD_RENDER)) continue;

        // Start new line if needed
        if (offset_x + glyph->bitmap.width + 1 >= 1024)
        {
            offset_y += row_height;
            row_height = 0;
            offset_x = 0;
        }

        // Load texture
        glTexSubImage2D(GL_TEXTURE_2D, 0, offset_x, offset_y, glyph->bitmap.width, glyph->bitmap.rows,
                        GL_ALPHA, GL_UNSIGNED_BYTE, glyph->bitmap.buffer);

        atlas->chars[i].advance_x = glyph->advance.x >> 6;
        atlas->chars[i].advance_y = glyph->advance.y >> 6;
        atlas->chars[i].width = glyph->bitmap.width;
        atlas->chars[i].height = glyph->bitmap.rows;
        atlas->chars[i].left = glyph->bitmap_left;
        atlas->chars[i].top = glyph->bitmap_top;
        atlas->chars[i].tex_x = offset_x / (float)atlas->width;
        atlas->chars[i].tex_y = offset_y / (float)atlas->height;

        row_height = MAX(row_height, glyph->bitmap.rows);
        offset_x += glyph->bitmap.width + 1;
    }

    // Clean FreeType
    FT_Done_FreeType(ft);

    return atlas;
}

drawable_t *graphics_label_create(graphics_t *g)
{
    (void)g;

    drawable_t *d = malloc(sizeof(struct _drawable));
    assert(d != NULL);

    d->type = DRAWABLE_LABEL;
    d->mask[0] = 0;
    d->mask[1] = 0;
    d->mask[2] = 0;
    d->mask[3] = 0;
    d->color[0] = 0;
    d->color[1] = 0;
    d->color[2] = 0;
    d->color[3] = 0;
    d->num = 0;

    glGenBuffers(1, &d->vbo);
    return d;
}

drawable_t *graphics_image_create(graphics_t *g, uint32_t width, uint32_t height)
{
    assert(g != NULL);

    drawable_t *d = malloc(sizeof(struct _drawable));
    assert(d != NULL);

    d->type = DRAWABLE_IMAGE;
    d->mask[0] = 1;
    d->mask[1] = 1;
    d->mask[2] = 1;
    d->mask[3] = 0;
    d->color[0] = 0;
    d->color[1] = 0;
    d->color[2] = 0;
    d->color[3] = 1;
    d->num = 6;

    // Calculate verticies
    float right = 2.0 / g->width * width;
    float bottom = 2.0 / g->height * height;
    GLfloat array[] =
    {
        0, 0, 0, 0,
        right, 0, 1, 0,
        0, -bottom, 0, 1,
        right, 0, 1, 0,
        0, -bottom, 0, 1,
        right, -bottom, 1, 1
    };

    // Buffer data
    glGenTextures(1, &d->tex);
    glGenBuffers(1, &d->vbo);
    glBindBuffer(GL_ARRAY_BUFFER, d->vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(array), array, GL_DYNAMIC_DRAW);

    return d;
}

void graphics_label_set_text(graphics_t *g, drawable_t *d, atlas_t *atlas, const char *text)
{
    assert(g != NULL);
    assert(d != NULL);
    assert(atlas != NULL);
    assert(text != NULL);
    assert(d->type == DRAWABLE_LABEL);

    int num = 0;
    float pos_x = 0;
    float pos_y = 0;
    float scale_x = 2.0 / g->width;
    float scale_y = 2.0 / g->height;

    GLfloat array[4 * 6 * strlen(text)];

    uint8_t *pc = (uint8_t*)text;
    for(; *pc; pc++)
    {
        uint8_t c = *pc - 32;
        float left = pos_x + atlas->chars[c].left * scale_x;
        float top = pos_y - atlas->chars[c].top * scale_y;
        float width = atlas->chars[c].width * scale_x;
        float height = atlas->chars[c].height * scale_y;

        pos_x += atlas->chars[c].advance_x * scale_x;
        pos_y += atlas->chars[c].advance_y * scale_y;
        if (!width || !height) continue;

        array[num + 0] = left;
        array[num + 1] = -top;
        array[num + 2] = atlas->chars[c].tex_x;
        array[num + 3] = atlas->chars[c].tex_y;
        num += 4;

        array[num + 0] = left + width;
        array[num + 1] = -top;
        array[num + 2] = atlas->chars[c].tex_x + atlas->chars[c].width / atlas->width;
        array[num + 3] = atlas->chars[c].tex_y;
        num += 4;

        array[num + 0] = left;
        array[num + 1] = -top - height;
        array[num + 2] = atlas->chars[c].tex_x;
        array[num + 3] = atlas->chars[c].tex_y + atlas->chars[c].height / atlas->height;
        num += 4;

        array[num + 0] = left + width;
        array[num + 1] = -top;
        array[num + 2] = atlas->chars[c].tex_x + atlas->chars[c].width / atlas->width;
        array[num + 3] = atlas->chars[c].tex_y;
        num += 4;

        array[num + 0] = left;
        array[num + 1] = -top - height;
        array[num + 2] = atlas->chars[c].tex_x;
        array[num + 3] = atlas->chars[c].tex_y + atlas->chars[c].height / atlas->height;
        num += 4;

        array[num + 0] = left + width;
        array[num + 1] = -top - height;
        array[num + 2] = atlas->chars[c].tex_x + atlas->chars[c].width / atlas->width;
        array[num + 3] = atlas->chars[c].tex_y + atlas->chars[c].height / atlas->height;
        num += 4;
    }

    // Buffer data
    glBindBuffer(GL_ARRAY_BUFFER, d->vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(array), array, GL_DYNAMIC_DRAW);
    d->num = num / 4;
}

void graphics_label_set_color(graphics_t *g, drawable_t *d, uint8_t color[4])
{
    (void)g;

    assert(d != NULL);
    assert(color != NULL);

    d->color[0] = color[0] / 255.0;
    d->color[1] = color[1] / 255.0;
    d->color[2] = color[2] / 255.0;
    d->mask[3] = color[3] / 255.0;
}

void graphics_image_set_bitmap(graphics_t *g, drawable_t *d, const void *buffer)
{
    assert(d != NULL);
    assert(buffer != NULL);
    assert(d->type == DRAWABLE_IMAGE);

    glBindTexture(GL_TEXTURE_2D, d->tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, g->width, g->height, 0, GL_RGBA, GL_UNSIGNED_BYTE, buffer);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
}

void graphics_drawable_free(drawable_t *d)
{
    assert(d != NULL);

    glDeleteBuffers(1, &d->vbo);
    if(d->type == DRAWABLE_IMAGE) glDeleteTextures(1, &d->tex);
    free(d);
}

void graphics_atlas_free(atlas_t *atlas)
{
    assert(atlas != NULL);

    glDeleteTextures(1, &atlas->texture);
    free(atlas);
}

void graphics_free(graphics_t *g)
{
    assert(g != NULL);

    // Delete shader
    glDeleteProgram(g->prog);
    glDeleteShader(g->vert);
    glDeleteShader(g->frag);

    // Release context and surface
    eglMakeCurrent(g->display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    eglDestroyContext(g->display, g->context);
    eglDestroySurface(g->display, g->surface);

    // Destroy window
    window_destroy(g->native_display, g->native_window);
}
