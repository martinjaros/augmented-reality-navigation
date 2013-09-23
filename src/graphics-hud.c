/*
 * Graphics library - HUD overlay
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
#include <stdio.h>

#include "debug.h"
#include "graphics.h"

#define GRAPHICS_PRIV_H
#include "graphics-priv.h"

/* Number of dashes on horizon line */
#define HORIZON_DASH_DIV    50

struct _hud
{
    float vfov;
    drawable_t *horizon_line;//, *compass_lines, *compass_labels[4];
    drawable_t *speed_label, *altitude_label, *waypoint_label;
    graphics_t *g;
};

static drawable_t *horizon_line_create(graphics_t *g, uint8_t color[4])
{
    struct _drawable *d = malloc(sizeof(struct _drawable));
    assert(d != 0);

    d->type = DRAWABLE_IMAGE;
    d->color[0] = color[0] / 255.0;
    d->color[1] = color[1] / 255.0;
    d->color[2] = color[2] / 255.0;
    d->color[3] = 0;
    d->mask[0] = 0;
    d->mask[1] = 0;
    d->mask[2] = 0;
    d->mask[3] = color[3] / 255.0;
    d->num = 2;
    d->mode = GL_LINES;

    // Generate texture
    GLchar buffer[4 * (HORIZON_DASH_DIV / 2 * 2 + 1)] = { };
    int i;
    for(i = 0; i < sizeof(buffer); i += 8) buffer[i + 3] = 0xFF;
    glGenTextures(1, &(d->tex));
    glBindTexture(GL_TEXTURE_2D, d->tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, sizeof(buffer) / 4, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, buffer);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    // Generate geometry
    GLfloat array[] =
    {
        -0.5, 0, 0, 0,
        0.5, 0, 1, 0,
    };
    glGenBuffers(1, &(d->vbo));
    glBindBuffer(GL_ARRAY_BUFFER, d->vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(array), array, GL_DYNAMIC_DRAW);

    return d;
}

hud_t *graphics_hud_create(graphics_t *g, atlas_t *atlas, uint8_t color[4], uint32_t font_size, float vfov)
{
    DEBUG("graphics_hud_create");
    assert(g != 0);
    assert(atlas != 0);
    assert(color != 0);

    hud_t *hud = calloc(1, sizeof(struct _hud));
    assert(hud != 0);

    hud->g = g;
    hud->vfov = vfov;
    hud->horizon_line = horizon_line_create(g, color);
    hud->speed_label = graphics_label_create(g, atlas, ANCHOR_LEFT_TOP);
    hud->altitude_label = graphics_label_create(g, atlas, ANCHOR_RIGHT_TOP);
    hud->waypoint_label = graphics_label_create(g, atlas, ANCHOR_CENTER_TOP);
    if(!hud->speed_label || !hud->altitude_label || !hud->waypoint_label)
    {
        WARN("Cannot create labels");
        goto error;
    }

    graphics_label_set_color(hud->speed_label, color);
    graphics_label_set_color(hud->altitude_label, color);
    graphics_label_set_color(hud->waypoint_label, color);

    return hud;

error:
    if(hud->horizon_line) graphics_drawable_free(hud->horizon_line);
    if(hud->speed_label) graphics_drawable_free(hud->speed_label);
    if(hud->altitude_label) graphics_drawable_free(hud->altitude_label);
    if(hud->waypoint_label) graphics_drawable_free(hud->waypoint_label);
    free(hud);
    return NULL;
}

void graphics_hud_draw(hud_t *hud, float attidude[3], float speed, float altitude, float track, float bearing, float distance, const char *waypoint)
{
    DEBUG("graphics_hud_draw");
    assert(hud != 0);
    assert(attidude != 0);
    assert(waypoint != 0);

    const char *wpt_name = waypoint[0] ? waypoint : "???";

    // Print labels
    char str[32];
    snprintf(str, sizeof(str), "%.0f km/h", speed);
    graphics_label_set_text(hud->speed_label, str);
    snprintf(str, sizeof(str), "%.0f m", altitude);
    graphics_label_set_text(hud->altitude_label, str);
    snprintf(str, sizeof(str), "%s, %.1f km", wpt_name, distance);
    graphics_label_set_text(hud->waypoint_label, str);

    // Draw
    graphics_draw(hud->g, hud->speed_label, 10, 10, 1, 0);
    graphics_draw(hud->g, hud->waypoint_label, hud->g->width / 2, 10, 1, 0);
    graphics_draw(hud->g, hud->altitude_label, hud->g->width - 10, 10, 1, 0);
    graphics_draw(hud->g, hud->horizon_line, hud->g->width / 2, (float)hud->g->height / 2 + (float)hud->g->height * attidude[1] / hud->vfov, 1, attidude[0]);
}

void graphics_hud_free(hud_t *hud)
{
    DEBUG("graphics_hud_free");
    assert(hud != 0);

    graphics_drawable_free(hud->horizon_line);
    graphics_drawable_free(hud->speed_label);
    graphics_drawable_free(hud->altitude_label);
    graphics_drawable_free(hud->waypoint_label);
    free(hud);
}
