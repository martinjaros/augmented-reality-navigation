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
#include <string.h>

#include "debug.h"
#include "graphics.h"
#include "graphics-utils.h"

struct _graphics
{
    /* EGL surface and its size */
    EGLDisplay display;
    EGLSurface surface;
    EGLContext context;
    EGLint width, height;

    /* GLSL shader program and attributes */
    GLuint vert, frag, prog;
    GLint attr_coord, uni_offset, uni_tex, uni_color, uni_mask;
};

struct _drawable
{
    // Drawable type
    enum { DRAWABLE_LABEL, DRAWABLE_IMAGE } type;

    // Vertex buffer
    GLuint vbo;

    // Vertex counter
    GLuint num;

    // Texture
    GLuint tex;

    // Colors
    GLfloat mask[4], color[4];
};

struct _drawable_image
{
    struct _drawable d;
    GLuint width, height;
};

struct _drawable_label
{
    struct _drawable d;
    graphics_t *g;
    atlas_t *atlas;
};

/* Resource cleanup helper function */
static void cleanup(graphics_t *g)
{
    // Delete shader
   if(g->prog) glDeleteProgram(g->prog);
   if(g->vert) glDeleteShader(g->vert);
   if(g->frag) glDeleteShader(g->frag);

   // Release context and surface
   if(g->surface)
   {
       eglMakeCurrent(g->display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
       eglDestroyContext(g->display, g->context);
       eglDestroySurface(g->display, g->surface);
   }

   free(g);
}

graphics_t *graphics_init(uint32_t window)
{
    DEBUG("graphics_init()");

    graphics_t *g = calloc(1, sizeof(graphics_t));
    if(g == NULL)
    {
        WARN("Cannot allocate memory");
        return NULL;
    }

    // Get display
    if((g->display = eglGetDisplay(EGL_DEFAULT_DISPLAY)) == NULL)
    {
        WARN("Failed to get display");
        cleanup(g);
        return NULL;
    }

    // Create surface
    if(!(g->surface = surface_create(g->display, window, &g->context)))
    {
        WARN("Cannot create surface");
        cleanup(g);
        return NULL;
    }

    // Compile shader
    #include "shaders.inc"
    if(!(g->vert = shader_compile(GL_VERTEX_SHADER, (GLchar*)src_shader_vert, src_shader_vert_len)) ||
       !(g->frag = shader_compile(GL_FRAGMENT_SHADER, (GLchar*)src_shader_frag, src_shader_frag_len)) ||
       !(g->prog = shader_link(g->vert, g->frag)))
    {
        WARN("Cannot compile shader");
        cleanup(g);
        return NULL;
    }

    // Get shader attributes
    glUseProgram(g->prog);
    g->uni_tex = glGetUniformLocation(g->prog, "tex");  // Texture
    g->uni_color = glGetUniformLocation(g->prog, "color"); // RGBA drawing color
    g->uni_mask = glGetUniformLocation(g->prog, "mask"); // RGBA color mask
    g->uni_offset = glGetUniformLocation(g->prog, "offset"); // X, Y drawing coordinates
    g->attr_coord = glGetAttribLocation(g->prog, "coord"); // Model vertex coordinates
    if((g->uni_tex == -1) || (g->uni_color == -1) || (g->uni_mask == -1) || (g->uni_offset == -1) || (g->attr_coord == -1))
    {
        WARN("Failed to get attribute locations");
        cleanup(g);
        return NULL;
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
        cleanup(g);
        return NULL;
    }

    // Check OpenGL errors
    int err = glGetError();
    if(err != GL_NO_ERROR)
    {
        WARN("OpenGL error %d", err);
        cleanup(g);
        return NULL;
    }

    return g;
}

uint32_t graphics_window_create(uint16_t width, uint16_t height)
{
    DEBUG("graphics_window_create()");
#ifdef X11BUILD

    // Open display
    Display *display = XOpenDisplay(NULL);
    if(!display)
    {
        WARN("Failed to open X display");
        return 0;
    }

    // Create window
    unsigned long color = BlackPixel(display, 0);
    Window root = RootWindow(display, 0);
    Window window = XCreateSimpleWindow(display, root, 0, 0, width, height, 0, color, color);
    XMapWindow(display, window);
    XFlush(display);
    INFO("Created window 0x%x", (uint32_t)window);
    return window;

#else /* X11BUILD */
    return (EGLNativeWindowType)0;
#endif
}

atlas_t *graphics_atlas_create(const char *font, uint32_t size)
{
    DEBUG("graphics_atlas_create()");
    assert(font != NULL);

    atlas_t *atlas = malloc(sizeof(struct _atlas));
    if(atlas == NULL)
    {
        WARN("Cannot allocate memory");
        return NULL;
    }

    if(!atlas_load_glyphs(atlas, font, size))
    {
        WARN("Cannot load glyphs");
        free(atlas);
        return NULL;
    }

    return atlas;
}

int graphics_flush(graphics_t *g, const uint8_t *color)
{
    DEBUG("graphics_flush()");
    assert(g != NULL);

    if(!eglSwapBuffers(g->display, g->surface)) return 0;
    if(color)
    {
        glClearColor(color[0] / 255.0, color[1] / 255.0, color[2] / 255.0, color[3] / 255.0);
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

void graphics_draw(graphics_t *g, drawable_t *d, uint32_t x, uint32_t y)
{
    DEBUG("graphics_draw()");
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

drawable_t *graphics_label_create(graphics_t *g, atlas_t *atlas)
{
    DEBUG("graphics_label_create()");
    assert(g != NULL);
    assert(atlas != NULL);

    struct _drawable_label *label = malloc(sizeof(struct _drawable_label));
    if(label == NULL)
    {
        WARN("Cannot allocate memory");
        return NULL;
    }

    label->d.type = DRAWABLE_LABEL;
    label->d.mask[0] = 0;
    label->d.mask[1] = 0;
    label->d.mask[2] = 0;
    label->d.mask[3] = 1;
    label->d.color[0] = 0;
    label->d.color[1] = 0;
    label->d.color[2] = 0;
    label->d.color[3] = 0;
    label->d.num = 0;

    glGenBuffers(1, &label->d.vbo);
    label->d.tex = atlas->texture;
    label->atlas = atlas;
    label->g = g;

    return (drawable_t*)label;
}

drawable_t *graphics_image_create(graphics_t *g, uint32_t width, uint32_t height)
{
    DEBUG("graphics_image_create()");
    assert(g != NULL);

    struct _drawable_image *image = malloc(sizeof(struct _drawable_image));
    if(image == NULL)
    {
        WARN("Cannot allocate memory");
        return NULL;
    }

    image->d.type = DRAWABLE_IMAGE;
    image->d.mask[0] = 1;
    image->d.mask[1] = 1;
    image->d.mask[2] = 1;
    image->d.mask[3] = 0;
    image->d.color[0] = 0;
    image->d.color[1] = 0;
    image->d.color[2] = 0;
    image->d.color[3] = 1;
    image->d.num = 6;

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
    glGenTextures(1, &(image->d.tex));
    glGenBuffers(1, &(image->d.vbo));
    glBindBuffer(GL_ARRAY_BUFFER, image->d.vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(array), array, GL_DYNAMIC_DRAW);
    image->width = width;
    image->height = height;

    return (drawable_t*)image;
}

void graphics_label_set_text(drawable_t *d, enum text_anchor anchor, const char *text)
{
    DEBUG("graphics_label_set_text()");
    assert(d != NULL);
    assert(text != NULL);
    assert(d->type == DRAWABLE_LABEL);
    struct _drawable_label *label = (struct _drawable_label*)d;

    GLfloat array[strlen(text) * 24];
    GLuint num = atlas_get_geometry(label->atlas, array, 2.0 / label->g->width, 2.0 / label->g->height, text);

    // Calculate offset for anchor
    if(num && (anchor != ANCHOR_LEFT_BOTTOM))
    {
        float offset_x = 0, offset_y = 0;
        switch(anchor)
        {
            case ANCHOR_LEFT_BOTTOM:
            case ANCHOR_LEFT_TOP:
                offset_y = array[num - 3] - array[1];
                break;

            case ANCHOR_CENTER_TOP:
                offset_x = (array[num - 4] - array[0]) / 2.0;
                offset_y = array[num - 3] - array[1];
                break;

            case ANCHOR_RIGHT_TOP:
                offset_x = array[num - 4] - array[0];
                offset_y = array[num - 3] - array[1];
                break;

            case ANCHOR_RIGHT_BOTTOM:
                offset_x = array[num - 4] - array[0];
                break;

            case ANCHOR_CENTER_BOTTOM:
                offset_x = (array[num - 4] - array[0]) / 2.0;
                break;
        }

        // Apply offset
        int i;
        for(i = 0; i < num / 4; i++)
        {
            array[i * 4 + 0] -= offset_x;
            array[i * 4 + 1] += offset_y;
        }
    }

    // Buffer array
    glBindBuffer(GL_ARRAY_BUFFER, label->d.vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(array), array, GL_DYNAMIC_DRAW);
    label->d.num = num / 4;
}

void graphics_label_set_color(drawable_t *d, uint8_t color[4])
{
    DEBUG("graphics_label_set_color()");

    assert(d != NULL);
    assert(color != NULL);

    d->color[0] = color[0] / 255.0;
    d->color[1] = color[1] / 255.0;
    d->color[2] = color[2] / 255.0;
    d->mask[3] = color[3] / 255.0;
}

void graphics_image_set_bitmap(drawable_t *d, const void *buffer)
{
    DEBUG("graphics_image_set_bitmap()");
    assert(d != NULL);
    assert(buffer != NULL);
    assert(d->type == DRAWABLE_IMAGE);

    struct _drawable_image *image = (struct _drawable_image*)d;
    glBindTexture(GL_TEXTURE_2D, image->d.tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, image->width, image->height, 0, GL_RGBA, GL_UNSIGNED_BYTE, buffer);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
}

void graphics_drawable_free(drawable_t *d)
{
    DEBUG("graphics_drawable_free()");
    assert(d != NULL);

    glDeleteBuffers(1, &d->vbo);
    if(d->type == DRAWABLE_IMAGE) glDeleteTextures(1, &d->tex);
    free(d);
}

void graphics_atlas_free(atlas_t *atlas)
{
    DEBUG("graphics_atlas_free()");
    assert(atlas != NULL);

    glDeleteTextures(1, &atlas->texture);
    free(atlas);
}

void graphics_free(graphics_t *g)
{
    DEBUG("graphics_free()");
    assert(g != NULL);

    cleanup(g);
}
