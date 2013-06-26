/*
 * Navigation utilities
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
#include <stdio.h>
#include <math.h>
#include <assert.h>

#include "nav.h"

/* Number of waypoints per chunk */
#define CHUNK_SIZE    64

/* I/O buffer size */
#define BUFFER_SIZE   128

/* Earth radius in meters */
#define EARTH_RADIUS  6371000.0

/* Waypoint */
struct _wpt
{
    char *label;
    struct posinfo pos;
};

/* Chained waypoint list */
struct _wpt_set
{
    size_t num;
    struct _wpt_set *next;
    struct _wpt wpts[CHUNK_SIZE];
};

/* Waypoint iterator */
struct _wpt_iter
{
    size_t index;
    struct _wpt_set *first;
    struct _wpt_set *current;
};

/* Resource cleanup helper function */
static void cleanup(struct _wpt_set *wpts)
{
    // Recurse to sub chunks
    if(wpts->next != NULL) cleanup(wpts->next);

    int i;
    for(i = 0; i < wpts->num; i++)
    {
        // Free string
        if(wpts->wpts[i].label != NULL) free(wpts->wpts[i].label);
    }

    // Free structure
    free(wpts);
}

wpt_iter *nav_load(const char *filename)
{
    FILE *f;
    wpt_iter *iter;

    assert(filename != NULL);

    // Open file
    if((f = fopen(filename, "r")) == NULL) return NULL;

    // Allocate memory for first chunk
    struct _wpt_set *set = malloc(sizeof(struct _wpt_set));
    set->next = NULL;
    set->num = 0;

    // Allocate iterator and set pointer to the first chunk
    iter = malloc(sizeof(struct _wpt_iter));
    iter->first = iter->current = set;
    iter->index = 0;

    char buf[BUFFER_SIZE];
    while(fgets(buf, BUFFER_SIZE, f) != NULL)
    {
        if(set->num >= CHUNK_SIZE)
        {
            // Allocate next chunk if needed
            set = set->next = malloc(sizeof(struct _wpt_set));
            set->next = NULL;
            set->num = 0;
        }

        struct _wpt *wpt = &set->wpts[set->num];
        char *str = buf;

        // Skip empty lines and '#' comments
        while(*str == ' ') str++;
        if((*str == '\n') || (*str == '#')) continue;

        // Parse
        if(sscanf(str, "%lf, %lf, %lf, %m[^\n]", &wpt->pos.lat, &wpt->pos.lon, &wpt->pos.alt, &wpt->label) != 4)
        {
            // Cleanup on error
            fclose(f);
            cleanup(iter->first);
            free(iter);
            return NULL;
        }

        // Increment counter
        set->num++;
    }

    fclose(f);
    return iter;
}

void nav_reset(wpt_iter *iter)
{
    assert(iter != NULL);

    // Reset iterator
    iter->index = 0;
    iter->current = iter->first;
}

int nav_iter(wpt_iter *iter, const struct attdinfo *attd, const struct posinfo *pos, struct wptinfo *res)
{
    assert(iter != NULL);
    assert(attd != NULL);
    assert(pos != NULL);
    assert(res != NULL);

    // Switch chunk if needed
    while(iter->index >= iter->current->num)
    {
        if(iter->current->next == NULL) return 0;
        iter->current = iter->current->next;
    }

    // Get current waypoint from iterator
    struct _wpt *wpt = &iter->current->wpts[iter->index++];

    // Calculate differentials
    double dalt = wpt->pos.alt - pos->alt;
    double dlat = wpt->pos.lat - pos->lat;
    double dlon = cos(pos->lat) * (wpt->pos.lon - pos->lon);

    // Calculate distance
    double dist = sqrt(dlat*dlat + dlon*dlon) * EARTH_RADIUS;

    // Calculate projection angles
    double hangle = atan2(dlon, dlat) - attd->yaw;
    double vangle = atan(dalt / dist) - attd->pitch;

    // Rotate
    double cosz = cos(attd->roll);
    double sinz = sin(attd->roll);

    res->x = hangle * cosz - vangle * sinz;
    res->y = hangle * sinz - vangle * cosz;
    res->label = wpt->label;

    return 1;
}

void nav_free(wpt_iter *iter)
{
    assert(iter != NULL);

    // Cleanup
    cleanup(iter->first);
    free(iter);
}
