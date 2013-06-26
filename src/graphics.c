/*
 * Graphics utilities
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
#include <string.h>
#include <assert.h>

// FreeType 2
#include <ft2build.h>
#include FT_FREETYPE_H

// OpenGL ES 2
#include <EGL/egl.h>
#include <GLES2/gl2.h>

#ifdef X11BUILD
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#endif

#include "graphics.h"

/* Helper macro, returns higher of the two arguments */
#define MAX(a,b) (((a)>(b))?(a):(b))

/* Clean & return helper macro */
#define clean_return(g) { graphics_free(g); return NULL; }

// Atlas texture width
#define TEXTURE_WIDTH  1024

/* Single character from the atlas */
struct _character
{
    // Advance
    float advance_x;
    float advance_y;

    // Bitmap
    float width;
    float height;
    float left;
    float top;

    // Texture
    float offset_x;
    float offset_y;
};

/* Atlas of characters */
struct _atlas
{
    // Texture and vertex buffer
    GLuint tex, vbo;
    int width, height;

    // Characters
    struct _character chars[128];
};

/* Internal state structure */
struct _graphics
{
    // EGL handles
    EGLDisplay display;
    EGLSurface surface;

    // Shader program
    GLuint vert, frag, program;

    // Texture and vertex buffer
    GLuint tex, vbo;

    // Atributes and uniforms
    GLint attribute_coord;
    GLint uniform_tex;
    GLint uniform_color;

    // Character atlas
    struct _atlas atlas;

    // Native handles
    EGLNativeDisplayType native_display;
    EGLNativeWindowType native_window;

    // Window size
    EGLint width, height;
};

/* EGL error check, returns true on error */
static int check_error(graphics_t *g)
{
    EGLint err = eglGetError();
    if(err != EGL_SUCCESS)
    {
        fprintf(stderr, "EGL error %d\n", err);
        return 1;
    }
    return 0;
}

/* Creates native window */
static int window_create(graphics_t *g)
{
#ifdef X11BUILD
    // Initialize the display
    if((g->native_display = XOpenDisplay(0)) == NULL)
    {
        fprintf(stderr, "Unable to open X display\n");
        return 0;
    }

    // Gets window parameters
    int screen = XDefaultScreen(g->native_display);
    int width  = XDisplayWidth(g->native_display, screen);
    int height = XDisplayHeight(g->native_display, screen);
    int black = XBlackPixel(g->native_display, screen);
    int white = XWhitePixel(g->native_display, screen);

    // Create window
    Window root = RootWindow(g->native_display, screen);
    g->native_window = XCreateSimpleWindow(g->native_display, root, 0, 0, width, height, 0, black, white);
    XMapWindow(g->native_display, g->native_window);
    Atom atom = XInternAtom(g->native_display, "WM_DELETE_WINDOW", False);
    XSetWMProtocols(g->native_display, g->native_window, &atom, 1);
    XFlush(g->native_display);
#endif

    return 1;
}

/* Creates character atlas texture */
static int atlas_create(graphics_t *g, const char *font_name, int font_height)
{
    int i;

    FT_Library ft;
    if(FT_Init_FreeType(&ft))
    {
        fprintf(stderr, "Cannot init freetype\n");
        return 0;
    }

    FT_Face face;
    if(FT_New_Face(ft, font_name, 0, &face))
    {
        fprintf(stderr, "Cannot open %s\n", font_name);
        return 0;
    }

    FT_Set_Pixel_Sizes(face, 0, font_height);

    FT_GlyphSlot glyph = face->glyph;
    int row_width = 0, row_height = 0;

    // Calculate texture size
    g->atlas.width = 0;
    g->atlas.height = 0;
    for (i = 32; i < 128; i++)
    {
        if(FT_Load_Char(face, i, FT_LOAD_RENDER)) continue;

        // Start new line if needed
        if(row_width + glyph->bitmap.width + 1 >= TEXTURE_WIDTH)
        {
            g->atlas.width = MAX(g->atlas.width, row_width);
            g->atlas.height += row_height;
            row_width = 0;
            row_height = 0;
        }

        row_width += glyph->bitmap.width + 1;
        row_height = MAX(row_height, glyph->bitmap.rows);
    }
    g->atlas.width = MAX(g->atlas.width, row_width);
    g->atlas.height += row_height;

    // Create texture
    glGenTextures(1, &g->atlas.tex);
    glBindTexture(GL_TEXTURE_2D, g->atlas.tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_ALPHA, g->atlas.width, g->atlas.height, 0, GL_ALPHA, GL_UNSIGNED_BYTE, 0);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    int offset_x = 0;
    int offset_y = 0;

    // Load characters
    row_height = 0;
    for(i = 32; i < 128; i++)
    {
        if (FT_Load_Char(face, i, FT_LOAD_RENDER)) continue;

        // Start new line if needed
        if (offset_x + glyph->bitmap.width + 1 >= TEXTURE_WIDTH)
        {
            offset_y += row_height;
            row_height = 0;
            offset_x = 0;
        }

        // Load texture
        glTexSubImage2D(GL_TEXTURE_2D, 0, offset_x, offset_y, glyph->bitmap.width, glyph->bitmap.rows,
                        GL_ALPHA, GL_UNSIGNED_BYTE, glyph->bitmap.buffer);

        g->atlas.chars[i].advance_x = glyph->advance.x >> 6;
        g->atlas.chars[i].advance_y = glyph->advance.y >> 6;
        g->atlas.chars[i].width = glyph->bitmap.width;
        g->atlas.chars[i].height = glyph->bitmap.rows;
        g->atlas.chars[i].left = glyph->bitmap_left;
        g->atlas.chars[i].top = glyph->bitmap_top;
        g->atlas.chars[i].offset_x = offset_x / (float)g->atlas.width;
        g->atlas.chars[i].offset_y = offset_y / (float)g->atlas.height;

        row_height = MAX(row_height, glyph->bitmap.rows);
        offset_x += glyph->bitmap.width + 1;
    }

    // Create the vertex buffer
    glGenBuffers(1, &g->atlas.vbo);

    return 1;
}

void graphics_free(graphics_t *g)
{
    assert(g != NULL);

    // Delete shaders
    if(g->program) glDeleteProgram(g->program);
    if(g->frag) glDeleteShader(g->frag);
    if(g->vert) glDeleteShader(g->vert);

    // Delete VBOs and textures
    if(g->vbo) glDeleteBuffers(1, &g->vbo);
    if(g->tex) glDeleteTextures(1, &g->tex);
    if(g->atlas.vbo) glDeleteBuffers(1, &g->vbo);
    if(g->atlas.tex) glDeleteTextures(1, &g->atlas.tex);

    // Close display
    if(g->display)
    {
        eglMakeCurrent(g->display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT) ;
        eglTerminate(g->display);
    }

#ifdef X11BUILD
    if (g->native_window) XDestroyWindow(g->native_display, g->native_window);
    if (g->native_display) XCloseDisplay(g->native_display);
#endif
}

graphics_t *graphics_init(const char *font_name, int font_height)
{
    // Allocate structure and create native window
    graphics_t *g = calloc(1, sizeof(struct _graphics));
    if(!window_create(g)) clean_return(g);

    EGLint major, minor;
    g->display = eglGetDisplay(g->native_display);
    if(!eglInitialize(g->display, &major, &minor)) clean_return(g);

    // Bind OpenGL API
    eglBindAPI(EGL_OPENGL_ES_API);
    if(check_error(g)) clean_return(g);

    EGLint num, config_attr[5];
    config_attr[0] = EGL_SURFACE_TYPE;
    config_attr[1] = EGL_WINDOW_BIT;
    config_attr[2] = EGL_RENDERABLE_TYPE;
    config_attr[3] = EGL_OPENGL_ES2_BIT;
    config_attr[4] = EGL_NONE;

    // Configure EGL
    EGLConfig config;
    if(!eglChooseConfig(g->display, config_attr, &config, 1, &num) || (num != 1)) clean_return(g);

    // Create surface
    g->surface = eglCreateWindowSurface(g->display, config, g->native_window, NULL);
    if(check_error(g)) clean_return(g);

    EGLint context_attr[] = { EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE };
    EGLContext context = eglCreateContext(g->display, config, NULL, context_attr);
    if(check_error(g)) clean_return(g);

    eglMakeCurrent(g->display, g->surface, g->surface, context);
    if(check_error(g)) clean_return(g);

    // Get surface size
    if(!eglQuerySurface(g->display, g->surface, EGL_WIDTH, &g->width)) clean_return(g);
    if(!eglQuerySurface(g->display, g->surface, EGL_HEIGHT, &g->height)) clean_return(g);

    // Include shader sources
    #include "shaders.inc"

    // Shader compilation status
    GLint status;

    // Compile vertex shader
    g->vert = glCreateShader(GL_VERTEX_SHADER);
    const GLchar* src_shader_vert_ptr = (char*)src_shader_vert;
    glShaderSource(g->vert, 1, &src_shader_vert_ptr, (int*)&src_shader_vert_len);
    glCompileShader(g->vert);
    glGetShaderiv(g->vert, GL_COMPILE_STATUS, &status);
    if(!status)
    {
        int max_len, len;
        glGetShaderiv(g->vert, GL_INFO_LOG_LENGTH, &max_len);
        char* str = malloc(max_len);
        glGetShaderInfoLog(g->vert, max_len, &len, str);
        fprintf(stderr, "Vertex shader: %s\n", str);
        free(str);
        clean_return(g);
    }

    // Compile fragment shader
    g->frag = glCreateShader(GL_FRAGMENT_SHADER);
    const GLchar* src_shader_frag_ptr = (char*)src_shader_frag;
    glShaderSource(g->frag, 1, &src_shader_frag_ptr, (int*)&src_shader_frag_len);
    glCompileShader(g->frag);
    glGetShaderiv(g->frag, GL_COMPILE_STATUS, &status);
    if(!status)
    {
        int max_len, len;
        glGetShaderiv(g->frag, GL_INFO_LOG_LENGTH, &max_len);
        char* str = malloc(max_len);
        glGetShaderInfoLog(g->frag, max_len, &len, str);
        fprintf(stderr, "Fragment shader: %s\n", str);
        free(str);
        clean_return(g);
    }

    // Link shaders
    g->program = glCreateProgram();
    glAttachShader(g->program, g->frag);
    glAttachShader(g->program, g->vert);
    glLinkProgram(g->program);
    glGetProgramiv(g->program, GL_LINK_STATUS, &status);
    if(!status)
    {
        int max_len, len;
        glGetProgramiv(g->program, GL_INFO_LOG_LENGTH, &max_len);
        char* str = malloc(max_len);
        glGetProgramInfoLog(g->program, max_len, &len, str);
        fprintf(stderr, "Shader program: %s\n", str);
        free(str);
        clean_return(g);
    }
    glUseProgram(g->program);

    // Get attributes and uniforms
    g->attribute_coord = glGetAttribLocation(g->program, "coord");
    g->uniform_tex = glGetUniformLocation(g->program, "tex");
    g->uniform_color = glGetUniformLocation(g->program, "color");

    // Create character atlas texture
    if(!atlas_create(g, font_name, font_height)) clean_return(g);

    // Create texture and VBO
    glGenTextures(1, &g->tex);
    glGenBuffers(1, &g->vbo);

    // Enable alpha channel
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    if(check_error(g)) clean_return(g);

    return g;
}

void graphics_draw_text(graphics_t *g, const char *str, uint8_t color[4], uint32_t x, uint32_t y)
{
    assert(g != NULL);

    // Set drawing color
    GLfloat col[4] = { color[0] / 255.0, color[1] / 255.0, color[2] / 255.0, color[3] / 255.0 };
    glUniform4fv(g->uniform_color, 1, col);

    // Bind the texture
    glBindTexture(GL_TEXTURE_2D, g->atlas.tex);
    glUniform1i(g->uniform_tex, 0);

    // Bind the VBO
    glEnableVertexAttribArray(g->attribute_coord);
    glBindBuffer(GL_ARRAY_BUFFER, g->atlas.vbo);
    glVertexAttribPointer(g->attribute_coord, 4, GL_FLOAT, GL_FALSE, 0, 0);

    // Calculate coordinates
    int num = 0;
    float scale_x = 2.0 / g->width;
    float scale_y = 2.0 / g->height;
    float pos_x = x * scale_x - 1.0;
    float pos_y = y * scale_y - 1.0;

    // Create vertex array
    GLfloat array[4 * 6 * strlen(str)];
    uint8_t *c = (uint8_t*)str;
    for(; *c; c++)
    {
        float left = pos_x + g->atlas.chars[*c].left * scale_x;
        float top = pos_y - g->atlas.chars[*c].top * scale_y;
        float width = g->atlas.chars[*c].width * scale_x;
        float height = g->atlas.chars[*c].height * scale_y;

        pos_x += g->atlas.chars[*c].advance_x * scale_x;
        pos_y += g->atlas.chars[*c].advance_y * scale_y;
        if (!width || !height) continue;

        array[num + 0] = left;
        array[num + 1] = -top;
        array[num + 2] = g->atlas.chars[*c].offset_x;
        array[num + 3] = g->atlas.chars[*c].offset_y;
        num += 4;

        array[num + 0] = left + width;
        array[num + 1] = -top;
        array[num + 2] = g->atlas.chars[*c].offset_x + g->atlas.chars[*c].width / g->atlas.width;
        array[num + 3] = g->atlas.chars[*c].offset_y;
        num += 4;

        array[num + 0] = left;
        array[num + 1] = -top - height;
        array[num + 2] = g->atlas.chars[*c].offset_x;
        array[num + 3] = g->atlas.chars[*c].offset_y + g->atlas.chars[*c].height / g->atlas.height;
        num += 4;

        array[num + 0] = left + width;
        array[num + 1] = -top;
        array[num + 2] = g->atlas.chars[*c].offset_x + g->atlas.chars[*c].width / g->atlas.width;
        array[num + 3] = g->atlas.chars[*c].offset_y;
        num += 4;

        array[num + 0] = left;
        array[num + 1] = -top - height;
        array[num + 2] = g->atlas.chars[*c].offset_x;
        array[num + 3] = g->atlas.chars[*c].offset_y + g->atlas.chars[*c].height / g->atlas.height;
        num += 4;

        array[num + 0] = left + width;
        array[num + 1] = -top - height;
        array[num + 2] = g->atlas.chars[*c].offset_x + g->atlas.chars[*c].width / g->atlas.width;
        array[num + 3] = g->atlas.chars[*c].offset_y + g->atlas.chars[*c].height / g->atlas.height;
        num += 4;
    }

    // Draw array
    if(num > 0)
    {
        glBufferData(GL_ARRAY_BUFFER, num * sizeof(GLfloat), array, GL_DYNAMIC_DRAW);
        glDrawArrays(GL_TRIANGLES, 0, num / 4);
    }
}

void graphics_draw_image(graphics_t *g, const void *img, uint32_t width, uint32_t height, uint32_t x, uint32_t y)
{
    assert(g != NULL);

    // Create and bind the texture
    glBindTexture(GL_TEXTURE_2D, g->tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, img);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glUniform1i(g->uniform_tex, 0);

    // Set drawing color
    GLfloat col[4] = { 0, 0, 0, 1 };
    glUniform4fv(g->uniform_color, 1, col);

    // Bind the VBO
    glEnableVertexAttribArray(g->attribute_coord);
    glBindBuffer(GL_ARRAY_BUFFER, g->vbo);
    glVertexAttribPointer(g->attribute_coord, 4, GL_FLOAT, GL_FALSE, 0, 0);

    // Create vertex array
    width--;
    height--;
    float scale_x = 2.0 / g->width;
    float scale_y = 2.0 / g->height;
    float pos_x = x * scale_x - 1.0;
    float pos_y = y * scale_y - 1.0;
    GLfloat array[] =
    {
        pos_x, -pos_y, 0, 0,
        -pos_x, -pos_y, width, 0,
        pos_x, pos_y, 0, height,
        -pos_x, -pos_y, width, 0,
        pos_x, pos_y, 0, height,
        -pos_x, pos_y, width, height
    };

    // Draw array
    glBufferData(GL_ARRAY_BUFFER, sizeof(array), array, GL_DYNAMIC_DRAW);
    glDrawArrays(GL_TRIANGLES, 0, 6);
}

void graphics_clear(graphics_t *g, const uint8_t color[4])
{
    assert(g != NULL);

    glClearColor(color[0] / 255.0, color[1] / 255.0, color[2] / 255.0, color[3] / 255.0);
    glClear(GL_COLOR_BUFFER_BIT);
}

int graphics_flush(graphics_t *g)
{

#ifdef X11BUILD
    XEvent event;
    int i, num = XPending(g->native_display);
    for(i = 0; i < num; i++)
    {
        XNextEvent(g->native_display, &event);
        if(event.type == ClientMessage) return 0;
    }
#endif

    // Swap Buffers.
    eglSwapBuffers(g->display, g->surface);
    if(check_error(g)) return 0;

    return 1;
}

void graphics_get_dimensions(graphics_t *g, uint32_t *width, uint32_t *height)
{
    assert(g != NULL);

    *width = g->width;
    *height = g->height;
}
