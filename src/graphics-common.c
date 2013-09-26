/*
 * Graphics library - common utilities
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

#include "debug.h"
#include "graphics.h"

#define GRAPHICS_PRIV_H
#include "graphics-priv.h"

struct _drawable_image
{
    struct _drawable d;
    GLuint width, height;
};

void graphics_draw(graphics_t *g, drawable_t *d, int x, int y, float scale, float rotation)
{
    DEBUG("graphics_draw()");
    assert(g != 0);
    assert(d != 0);

    if(d->num == 0) return;

    // Set position, scale and rotation
    glUniform2f(g->uni_offset, (GLfloat)x * 2.0 / (GLfloat)g->width - 1.0, (GLfloat)y * -2.0 / (GLfloat)g->height + 1.0);
    glUniform2f(g->uni_scale, scale, scale);
    glUniform1f(g->uni_rot, rotation);

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
    glDrawArrays(d->mode, 0, d->num);
}

drawable_t *graphics_image_create(graphics_t *g, uint32_t width, uint32_t height, enum anchor_types anchor)
{
    DEBUG("graphics_image_create()");
    assert(g != 0);

    struct _drawable_image *image = malloc(sizeof(struct _drawable_image));
    assert(image != 0);

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
    image->d.mode = GL_TRIANGLES;

    float right = 2.0 / g->width * width;
    float bottom = 2.0 / g->height * height;
    float offset_x = 0;
    float offset_y = 0;
    switch(anchor)
    {
        case ANCHOR_LEFT_BOTTOM:
            offset_y = (float)bottom;
            break;

        case ANCHOR_LEFT_TOP:
            break;

        case ANCHOR_CENTER_TOP:
            offset_x = -(float)right / 2.0;
            break;

        case ANCHOR_RIGHT_TOP:
            offset_x = -(float)right;
            break;

        case ANCHOR_RIGHT_BOTTOM:
            offset_x = -(float)right;
            offset_y = (float)bottom;
            break;

        case ANCHOR_CENTER_BOTTOM:
            offset_x = -(float)right / 2.0;
            offset_y = (float)bottom;
            break;

        case ANCHOR_CENTER:
            offset_x = -(float)right / 2.0;
            offset_y = (float)bottom / 2.0;
            break;
    }

    // Calculate verticies
    GLfloat array[] =
    {
        offset_x,         offset_y,          0, 0,
        offset_x + right, offset_y,          1, 0,
        offset_x,         offset_y - bottom, 0, 1,
        offset_x + right, offset_y,          1, 0,
        offset_x,         offset_y - bottom, 0, 1,
        offset_x + right, offset_y - bottom, 1, 1
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

void graphics_image_set_bitmap(drawable_t *d, const void *buffer)
{
    DEBUG("graphics_image_set_bitmap()");
    assert(d != 0);
    assert(buffer != 0);
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
    assert(d != 0);

    glDeleteBuffers(1, &d->vbo);
    if(d->type != DRAWABLE_LABEL) glDeleteTextures(1, &d->tex);
    free(d);
}
