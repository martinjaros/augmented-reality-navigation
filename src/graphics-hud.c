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
#include <math.h>

#include "debug.h"
#include "graphics.h"

#define GRAPHICS_PRIV_H
#include "graphics-priv.h"

/* Number of dashes on the horizon line (odd number) */
#define HORIZON_DASH_NUM        51

/* Length of the horizon line */
#define HORIZON_LENGTH_REL      .5

/* Number of labels on compass tape */
#define COMPASS_LABEL_NUM       12

/* Number of steps on compass tape */
#define COMPASS_STEP_NUM        36

/* Compass tape height */
#define COMPASS_HEIGHT          10

/* Size of markers */
#define MARKER_SIZE             12

/* Number of verticies per circle */
#define CIRCLE_DIV              8

struct _hud
{
    float hfov, vfov;
    drawable_t *horizon_line, *horizon_alt, *track_marker, *bearing_marker, *compass_lines, *compass_labels[COMPASS_LABEL_NUM];
    drawable_t *speed_label, *altitude_label, *waypoint_label;
    graphics_t *g;
};

static drawable_t *horizon_create(graphics_t *g, uint8_t color[4])
{
    struct _drawable *d = malloc(sizeof(struct _drawable));
    assert(d != 0);

    d->type = DRAWABLE_BASE;
    d->color[0] = color[0] / 255.0;
    d->color[1] = color[1] / 255.0;
    d->color[2] = color[2] / 255.0;
    d->color[3] = 0;
    d->mask[0] = 0;
    d->mask[1] = 0;
    d->mask[2] = 0;
    d->mask[3] = color[3] / 255.0;
    d->num = 4;
    d->mode = GL_LINE_STRIP;

    // Generate texture
    GLchar buffer[] = { 0xFF, 0x00 };
    glGenTextures(1, &(d->tex));
    glBindTexture(GL_TEXTURE_2D, d->tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_ALPHA, sizeof(buffer), 1, 0, GL_ALPHA, GL_UNSIGNED_BYTE, buffer);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    // Generate geometry
    GLfloat array[] =
    {
        -HORIZON_LENGTH_REL - 0.005, -0.02, -2, 0,
        -HORIZON_LENGTH_REL, 0, 0, 0,
        HORIZON_LENGTH_REL, 0, HORIZON_DASH_NUM, 0,
        HORIZON_LENGTH_REL + 0.005, -0.02, HORIZON_DASH_NUM + 2
    };
    glGenBuffers(1, &(d->vbo));
    glBindBuffer(GL_ARRAY_BUFFER, d->vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(array), array, GL_DYNAMIC_DRAW);

    return d;
}

static drawable_t *horizon_alt_create(graphics_t *g, uint8_t color[4])
{
    struct _drawable *d = malloc(sizeof(struct _drawable));
    assert(d != 0);

    d->type = DRAWABLE_BASE;
    d->color[0] = color[0] / 255.0;
    d->color[1] = color[1] / 255.0;
    d->color[2] = color[2] / 255.0;
    d->color[3] = 0;
    d->mask[0] = 0;
    d->mask[1] = 0;
    d->mask[2] = 0;
    d->mask[3] = color[3] / 255.0;
    d->num = 6;
    d->mode = GL_LINES;

    // Generate texture
    GLchar buffer[] = { 0xFF, 0x00 };
    glGenTextures(1, &(d->tex));
    glBindTexture(GL_TEXTURE_2D, d->tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_ALPHA, sizeof(buffer), 1, 0, GL_ALPHA, GL_UNSIGNED_BYTE, buffer);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    // Generate geometry
    GLfloat array[] =
    {
        0, 0, 0, 0,
        0, -0.2, 10, 0,
        0, 0, 0, 0,
        0.05, -0.1, 5, 0,
        0, 0, 0, 0,
        -0.05, -0.1, 5, 0
    };

    glGenBuffers(1, &(d->vbo));
    glBindBuffer(GL_ARRAY_BUFFER, d->vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(array), array, GL_DYNAMIC_DRAW);

    return d;
}

static drawable_t *compass_create(graphics_t *g, uint8_t color[4], float hfov)
{
    struct _drawable *d = malloc(sizeof(struct _drawable));
    assert(d != 0);

    d->type = DRAWABLE_BASE;
    d->color[0] = color[0] / 255.0;
    d->color[1] = color[1] / 255.0;
    d->color[2] = color[2] / 255.0;
    d->color[3] = color[3] / 255.0;
    d->mask[0] = 0;
    d->mask[1] = 0;
    d->mask[2] = 0;
    d->mask[3] = 0;
    d->num = 2 + 2 * COMPASS_STEP_NUM;
    d->mode = GL_LINES;

    // Generate texture
    glGenTextures(1, &(d->tex));

    // Generate geometry
    GLfloat array[4 * (2 + 2 * COMPASS_STEP_NUM)] =
    {
        0, 0, 0, 0,
        4 * M_PI / hfov, 0, 0, 0
    };

    int i;
    float f;
    for(f = 0, i = 8; f < COMPASS_STEP_NUM; f++, i += 8)
    {
        array[i + 4] = array[i] = f * 4 * M_PI / COMPASS_STEP_NUM / hfov;
        array[i + 5] = -COMPASS_HEIGHT / (float)g->height * 2;
    }
    glGenBuffers(1, &(d->vbo));
    glBindBuffer(GL_ARRAY_BUFFER, d->vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(array), array, GL_DYNAMIC_DRAW);

    return d;
}

static drawable_t *marker1_create(graphics_t *g, uint8_t color[4])
{
    struct _drawable *d = malloc(sizeof(struct _drawable));
    assert(d != 0);

    d->type = DRAWABLE_BASE;
    d->color[0] = color[0] / 255.0;
    d->color[1] = color[1] / 255.0;
    d->color[2] = color[2] / 255.0;
    d->color[3] = color[3] / 255.0;
    d->mask[0] = 0;
    d->mask[1] = 0;
    d->mask[2] = 0;
    d->mask[3] = 0;
    d->num = 3;
    d->mode = GL_LINE_LOOP;

    // Generate texture
    glGenTextures(1, &(d->tex));

    // Generate geometry
    GLfloat array[] =
    {
        0, 0, 0, 0,
        -MARKER_SIZE / (float)g->width, MARKER_SIZE / (float)g->height * 2, 0, 0,
        MARKER_SIZE / (float)g->width, MARKER_SIZE / (float)g->height * 2, 0, 0
    };

    glGenBuffers(1, &(d->vbo));
    glBindBuffer(GL_ARRAY_BUFFER, d->vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(array), array, GL_DYNAMIC_DRAW);

    return d;
}

static drawable_t *marker2_create(graphics_t *g, uint8_t color[4])
{
    struct _drawable *d = malloc(sizeof(struct _drawable));
    assert(d != 0);

    d->type = DRAWABLE_BASE;
    d->color[0] = color[0] / 255.0;
    d->color[1] = color[1] / 255.0;
    d->color[2] = color[2] / 255.0;
    d->color[3] = color[3] / 255.0;
    d->mask[0] = 0;
    d->mask[1] = 0;
    d->mask[2] = 0;
    d->mask[3] = 0;
    d->num = CIRCLE_DIV;
    d->mode = GL_LINE_LOOP;

    // Generate texture
    glGenTextures(1, &(d->tex));

    // Generate geometry
    GLfloat array[4 * CIRCLE_DIV];
    int i;
    float f;
    for(i = 0, f = 0; f < CIRCLE_DIV; f++, i += 4)
    {
        array[i + 0] = COMPASS_HEIGHT / (float)g->width * cosf(f * 2 * M_PI / CIRCLE_DIV);
        array[i + 1] = -COMPASS_HEIGHT / (float)g->height + COMPASS_HEIGHT / (float)g->height * sinf(f * 2 * M_PI / CIRCLE_DIV);
        array[i + 2] = 0;
        array[i + 3] = 0;
    }

    glGenBuffers(1, &(d->vbo));
    glBindBuffer(GL_ARRAY_BUFFER, d->vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(array), array, GL_DYNAMIC_DRAW);

    return d;
}

hud_t *graphics_hud_create(graphics_t *g, atlas_t *atlas, uint8_t color[4], uint32_t font_size, float hfov, float vfov)
{
    DEBUG("graphics_hud_create()");
    assert(g != 0);
    assert(atlas != 0);
    assert(color != 0);

    hud_t *hud = calloc(1, sizeof(struct _hud));
    assert(hud != 0);

    hud->g = g;
    hud->hfov = hfov;
    hud->vfov = vfov;
    hud->horizon_line = horizon_create(g, color);
    hud->horizon_alt = horizon_alt_create(g, color);
    hud->compass_lines = compass_create(g, color, hfov);
    hud->track_marker = marker1_create(g, color);
    hud->bearing_marker = marker2_create(g, color);
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

    char str[4];
    int i;
    for(i = 0; i < COMPASS_LABEL_NUM; i++)
    {
        snprintf(str, sizeof(str), "%d", i * 360 / COMPASS_LABEL_NUM);
        if(!(hud->compass_labels[i] = graphics_label_create(g, atlas, ANCHOR_CENTER_BOTTOM)))
        {
            WARN("Cannot create labels");
            while(--i >= 0) graphics_drawable_free(hud->compass_labels[i]);
            goto error;
        }
        graphics_label_set_text(hud->compass_labels[i], str);
        graphics_label_set_color(hud->compass_labels[i], color);
    }

    return hud;

error:
    if(hud->horizon_line) graphics_drawable_free(hud->horizon_line);
    if(hud->horizon_alt) graphics_drawable_free(hud->horizon_alt);
    if(hud->compass_lines) graphics_drawable_free(hud->compass_lines);
    if(hud->track_marker) graphics_drawable_free(hud->track_marker);
    if(hud->bearing_marker) graphics_drawable_free(hud->bearing_marker);
    if(hud->speed_label) graphics_drawable_free(hud->speed_label);
    if(hud->altitude_label) graphics_drawable_free(hud->altitude_label);
    if(hud->waypoint_label) graphics_drawable_free(hud->waypoint_label);
    free(hud);
    return NULL;
}

void graphics_hud_draw(hud_t *hud, float attitude[3], float speed, float altitude, float track, float bearing, float distance, const char *waypoint)
{
    DEBUG("graphics_hud_draw()");
    assert(hud != 0);
    assert(attitude != 0);
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

    float hangle, vangle;

    // Draw static labels
    graphics_draw(hud->g, hud->speed_label, 10, 10, 1, 0);
    graphics_draw(hud->g, hud->waypoint_label, hud->g->width / 2, 10, 1, 0);
    graphics_draw(hud->g, hud->altitude_label, hud->g->width - 10, 10, 1, 0);

    // Draw horizon line
    vangle = -attitude[1];
    vangle = vangle < M_PI ? vangle : vangle - 2 * M_PI;
    vangle = vangle > -M_PI ? vangle : vangle + 2 * M_PI;
    if(vangle < hud->vfov / -2.0)
        graphics_draw(hud->g, hud->horizon_alt, hud->g->width / 2, 100, 1, 0);
    else if(vangle > hud->vfov / 2.0)
        graphics_draw(hud->g, hud->horizon_alt, hud->g->width / 2, (float)hud->g->height - 100, 1, M_PI);
    else
        graphics_draw(hud->g, hud->horizon_line, hud->g->width / 2, (float)hud->g->height / 2 + (float)hud->g->height * vangle / hud->vfov, 1, -attitude[0]);

    // Draw compass lines
    graphics_draw(hud->g, hud->compass_lines, (float)hud->g->width / 2 - (float)hud->g->width * attitude[2] / hud->hfov, hud->g->height - 30, 1, 0);
    if(attitude[2] < hud->hfov / 2) graphics_draw(hud->g, hud->compass_lines, (float)hud->g->width / 2 - (float)hud->g->width * (attitude[2] + 2 * M_PI)  / hud->hfov, hud->g->height - 30, 1, 0);
    if(attitude[2] > 2 * M_PI - hud->hfov / 2) graphics_draw(hud->g, hud->compass_lines, (float)hud->g->width / 2 - (float)hud->g->width * (attitude[2] - 2 * M_PI) / hud->hfov, hud->g->height - 30, 1, 0);

    // Draw compass labels
    int i;
    for(i = 0; i < COMPASS_LABEL_NUM; i++)
    {
        hangle = attitude[2] - i * 2 * M_PI / COMPASS_LABEL_NUM;
        hangle = hangle < M_PI ? hangle : hangle - 2 * M_PI;
        hangle = hangle > -M_PI ? hangle : hangle + 2 * M_PI;
        if((hangle < hud->hfov / -2.0) || (hangle > hud->hfov / 2.0)) continue;
        graphics_draw(hud->g, hud->compass_labels[i], (float)hud->g->width / 2 - (float)hud->g->width * hangle / hud->hfov, hud->g->height - 3, 1, 0);
    }

    // Draw track marker
    hangle = track - attitude[2];
    hangle = hangle < M_PI ? hangle : hangle - 2 * M_PI;
    hangle = hangle > -M_PI ? hangle : hangle + 2 * M_PI;
    if(hangle < hud->hfov / -2.0) hangle = hud->hfov / -2.0;
    if(hangle > hud->hfov / 2.0) hangle = hud->hfov / 2.0;
    graphics_draw(hud->g, hud->track_marker, (float)hud->g->width / 2 + (float)hud->g->width * hangle / hud->hfov, hud->g->height - 32, 1, 0);

    // Draw bearing marker
    hangle = bearing - attitude[2];
    hangle = hangle < M_PI ? hangle : hangle - 2 * M_PI;
    hangle = hangle > -M_PI ? hangle : hangle + 2 * M_PI;
    if(hangle < hud->hfov / -2.0) hangle = hud->hfov / -2.0;
    if(hangle > hud->hfov / 2.0) hangle = hud->hfov / 2.0;
    graphics_draw(hud->g, hud->bearing_marker, (float)hud->g->width / 2 + (float)hud->g->width * hangle / hud->hfov, hud->g->height - 30, 1, 0);
}

void graphics_hud_free(hud_t *hud)
{
    DEBUG("graphics_hud_free()");
    assert(hud != 0);

    int i;
    for(i = 0; i < COMPASS_LABEL_NUM; i++) graphics_drawable_free(hud->compass_labels[i]);
    graphics_drawable_free(hud->compass_lines);
    graphics_drawable_free(hud->track_marker);
    graphics_drawable_free(hud->bearing_marker);
    graphics_drawable_free(hud->horizon_line);
    graphics_drawable_free(hud->horizon_alt);
    graphics_drawable_free(hud->speed_label);
    graphics_drawable_free(hud->altitude_label);
    graphics_drawable_free(hud->waypoint_label);
    free(hud);
}
