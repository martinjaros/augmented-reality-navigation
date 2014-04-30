/*
 * GPS internal utilities
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

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include <png.h>

#include "debug.h"
#include "gps-util.h"

/* I/O Buffer size */
#define BUFFER_SIZE     256

struct waypoint_node *gps_util_load_datafile(const char *filename)
{
    DEBUG("gps_util_load_datafile");
    assert(filename != 0);

    FILE *fp = fopen(filename, "r");
    if(!fp)
    {
        WARN("Failed to open `%s`", filename);
        return NULL;
    }
    else
    {
        struct waypoint_node *node = NULL, *node_prev = NULL, *result = NULL;

        char buf[BUFFER_SIZE];
        while(fgets(buf, BUFFER_SIZE, fp))
        {
            char *str = buf;

            // Skip empty lines and '#' comments
            while(*str == ' ') str++;
            if((*str == '\n') || (*str == '#')) continue;
            str[strlen(str) - 1] = 0;
            INFO("Parsing landmark line `%s`", str);

            node = malloc(sizeof(struct waypoint_node));
            assert(node != 0);

            // Parse line
            if(sscanf(str, "%lf, %lf, %f, %32[^\n]", &node->lat, &node->lon, &node->alt, node->name) != 4)
            {
                WARN("Parse error");
                free(node);
                continue;
            }

            node->label = NULL;
            node->next = NULL;
            if(node_prev)
            {
                node_prev = node_prev->next = node;
            }
            else
            {
                result = node_prev = node;
            }
        }

        fclose(fp);
        return result;
    }
}

struct dem *gps_util_load_demfile(const char *filename, double left, double top, double right, double bottom, float scale)
{
    DEBUG("gps_util_load_demfile");
    assert(filename != 0);

    FILE *fp = fopen(filename, "rb");
    if(fp == NULL)
    {
        WARN("Cannot open `%s`", filename);
        return NULL;
    }

    struct dem *dem = malloc(sizeof(struct dem));
    assert(dem != 0);

    png_structp png = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    assert(png != 0);

    png_infop info = png_create_info_struct(png);
    assert(info != 0);

    png_init_io(png, fp);
    png_read_info(png, info);
    dem->width = png_get_image_width(png, info);
    dem->height = png_get_image_height(png, info);
    assert(png_get_rowbytes(png, info) == dem->width * 2);
    dem->lines = malloc(dem->height * sizeof(png_bytep));
    assert(dem->lines != 0);

    INFO("Loading heightmap");

    int i;
    for(i = 0; i < dem->height; i++)
    {
        dem->lines[i] = malloc(dem->width * 2);
        assert(dem->lines[i] != 0);
    }

    png_read_image(png, dem->lines);
    png_destroy_read_struct(&png, &info, NULL);

    dem->top = top;
    dem->left = left;
    dem->right = right;//height / (top - bottom);
    dem->bottom = bottom;//width / (right - left);
    dem->pixel_scale = scale / (float)0xFFFF;
    return dem;
}

float gps_util_dem_get_alt(struct dem *dem, double lat, double lon)
{
    DEBUG("gps_util_dem_get_alt");
    assert(dem != 0);

    int x = (int)((lon - dem->left) / (dem->right - dem->left) * dem->width + 0.5);
    int y = (int)((dem->top - lat) / (dem->top - dem->bottom) * dem->height + 0.5);
    if((x < 0) || (y < 0) || (x >= dem->width) || (y >= dem->height)) return 0;
    return (float)(((uint16_t)dem->lines[y][x * 2] << 8) | (uint16_t)dem->lines[y][x * 2 + 1]) * dem->pixel_scale;
}

