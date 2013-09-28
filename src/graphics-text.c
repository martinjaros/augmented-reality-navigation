/*
 * Graphics library - text utilities
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

#include <assert.h>

#include "debug.h"
#include "graphics.h"
#include "math.h"

#define GRAPHICS_PRIV_H
#include "graphics-priv.h"

#include <ft2build.h>
#include FT_FREETYPE_H

#define MIN(a,b) (((a)<(b))?(a):(b))
#define MAX(a,b) (((a)>(b))?(a):(b))

/* Atlas character mapping, using characters 32 - 255 of ISO-8859-1 / Unicode */
#define ATLAS_MAP_OFFSET 32
#define ATLAS_MAP_LENGTH (256 - ATLAS_MAP_OFFSET)

/* Atlas texture size */
#define ATLAS_TEXTURE_WIDTH 512
#define ATLAS_TEXTURE_HEIGHT 512

struct _drawable_label
{
    struct _drawable d;
    graphics_t *g;
    atlas_t *atlas;
    enum anchor_types anchor;
};

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

atlas_t *graphics_atlas_create(const char *font, uint32_t size)
{
    DEBUG("graphics_atlas_create()");
    assert(font != 0);

    atlas_t *atlas = malloc(sizeof(struct _atlas));
    assert(atlas != 0);

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
        atlas->chars[i].advance_x = (face->glyph->advance.x >> 6);
        atlas->chars[i].advance_y = (face->glyph->advance.y >> 6);
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
    return atlas;

error:
    FT_Done_FreeType(ft);
    free(atlas);
    return NULL;
}

drawable_t *graphics_label_create(graphics_t *g, atlas_t *atlas, enum anchor_types anchor)
{
    DEBUG("graphics_label_create()");
    assert(g != 0);
    assert(atlas != 0);

    struct _drawable_label *label = malloc(sizeof(struct _drawable_label));
    assert(label != 0);

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
    label->d.mode = GL_TRIANGLES;

    glGenBuffers(1, &label->d.vbo);
    label->d.tex = atlas->texture;
    label->atlas = atlas;
    label->g = g;
    label->anchor = anchor;

    return (drawable_t*)label;
}

void graphics_label_set_text(drawable_t *d, const char *text)
{
    DEBUG("graphics_label_set_text()");
    assert(d != 0);
    assert(text != 0);
    assert(d->type == DRAWABLE_LABEL);
    struct _drawable_label *label = (struct _drawable_label*)d;

    GLfloat array[strlen(text) * 24];
    GLuint num = 0;

    float row_top = 0, row_bottom = 0;
    float scale_x = 2.0 / label->g->width;
    float scale_y = 2.0 / label->g->height;
    float pos_x = 0;
    float pos_y = 0;

    // Calculate geometry
    uint8_t *pc = (uint8_t*)text;
    for(; *pc; pc++)
    {
        uint8_t c = *pc - ATLAS_MAP_OFFSET;
        if(c >= ATLAS_MAP_LENGTH) continue;
        float left = pos_x + label->atlas->chars[c].left * scale_x;
        float bottom = pos_y + label->atlas->chars[c].top * scale_y;
        float width = label->atlas->chars[c].width * scale_x;
        float height = label->atlas->chars[c].height * scale_y;

        pos_x += label->atlas->chars[c].advance_x * scale_x;
        pos_y += label->atlas->chars[c].advance_y * scale_y;
        if (!width || !height) continue;

        row_top = MIN(row_bottom, bottom - height);
        row_bottom = MAX(row_top, bottom);

        // Left bottom
        array[num + 0] = left;
        array[num + 1] = bottom;
        array[num + 2] = label->atlas->chars[c].tex_x;
        array[num + 3] = label->atlas->chars[c].tex_y;
        num += 4;

        // Right bottom
        array[num + 0] = left + width;
        array[num + 1] = bottom;
        array[num + 2] = label->atlas->chars[c].tex_x + label->atlas->chars[c].width / ATLAS_TEXTURE_WIDTH;
        array[num + 3] = label->atlas->chars[c].tex_y;
        num += 4;

        // Left top
        array[num + 0] = left;
        array[num + 1] = bottom - height;
        array[num + 2] = label->atlas->chars[c].tex_x;
        array[num + 3] = label->atlas->chars[c].tex_y + label->atlas->chars[c].height / ATLAS_TEXTURE_HEIGHT;
        num += 4;

        // Right bottom
        array[num + 0] = left + width;
        array[num + 1] = bottom;
        array[num + 2] = label->atlas->chars[c].tex_x + label->atlas->chars[c].width / ATLAS_TEXTURE_WIDTH;
        array[num + 3] = label->atlas->chars[c].tex_y;
        num += 4;

        // Left top
        array[num + 0] = left;
        array[num + 1] = bottom - height;
        array[num + 2] = label->atlas->chars[c].tex_x;
        array[num + 3] = label->atlas->chars[c].tex_y + label->atlas->chars[c].height / ATLAS_TEXTURE_HEIGHT;
        num += 4;

        // Right top
        array[num + 0] = left + width;
        array[num + 1] = bottom - height;
        array[num + 2] = label->atlas->chars[c].tex_x + label->atlas->chars[c].width / ATLAS_TEXTURE_WIDTH;
        array[num + 3] = label->atlas->chars[c].tex_y + label->atlas->chars[c].height / ATLAS_TEXTURE_HEIGHT;
        num += 4;
    }

    // Calculate offset for anchor
    if(num && (label->anchor != ANCHOR_LEFT_BOTTOM))
    {
        float offset_x = 0, offset_y = 0;
        switch(label->anchor)
        {
            case ANCHOR_LEFT_BOTTOM:
            case ANCHOR_LEFT_TOP:
                offset_y = row_bottom - row_top;
                break;

            case ANCHOR_CENTER_TOP:
                offset_x = (array[num - 4] - array[0]) / 2.0;
                offset_y = row_bottom - row_top;
                break;

            case ANCHOR_RIGHT_TOP:
                offset_x = (array[num - 4] - array[0]);
                offset_y = row_bottom - row_top;
                break;

            case ANCHOR_RIGHT_BOTTOM:
                offset_x = (array[num - 4] - array[0]);
                break;

            case ANCHOR_CENTER_BOTTOM:
                offset_x = (array[num - 4] - array[0]) / 2.0;
                break;

            case ANCHOR_CENTER:
                offset_x = (array[num - 4] - array[0]) / 2.0;
                offset_y = (row_bottom - row_top) / 2.0;
        }

        // Apply offset
        int i;
        for(i = 0; i < num / 4; i++)
        {
            array[i * 4 + 0] -= offset_x - fmodf(offset_x, scale_x);
            array[i * 4 + 1] -= offset_y - fmodf(offset_y, scale_y);
        }
    }

    // Buffer array
    glBindBuffer(GL_ARRAY_BUFFER, label->d.vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(array), array, GL_DYNAMIC_DRAW);
    label->d.num = num / 4;
}

void graphics_label_set_color(drawable_t *d, const uint8_t color[4])
{
    DEBUG("graphics_label_set_color()");
    assert(d != 0);
    assert(color != 0);
    assert(d->type == DRAWABLE_LABEL);

    d->color[0] = color[0] / 255.0;
    d->color[1] = color[1] / 255.0;
    d->color[2] = color[2] / 255.0;
    d->mask[3] = color[3] / 255.0;
}

void graphics_atlas_free(atlas_t *atlas)
{
    DEBUG("graphics_atlas_free()");
    assert(atlas != 0);

    glDeleteTextures(1, &atlas->texture);
    free(atlas);
}
