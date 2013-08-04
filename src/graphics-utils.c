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

EGLNativeWindowType window_create(EGLNativeDisplayType *display)
{
    DEBUG("window_create()");
#ifdef X11BUILD

    // Open display
    *display = XOpenDisplay(NULL);
    if(!*display)
    {
        WARN("Failed to open X display");
        return 0;
    }

    // Create window
    unsigned long color = BlackPixel(*display, 0);
    Window root = RootWindow(*display, 0);
    Window window = XCreateSimpleWindow(*display, root, 0, 0, 100, 100, 0, color, color);
    Atom state = XInternAtom(*display, "_NET_WM_STATE", False);
    Atom fullscreen = XInternAtom(*display, "_NET_WM_STATE_FULLSCREEN", False);
    if(!window)
    {
        WARN("Failed to create X11 window");
        return 0;
    }

    XEvent event;
    memset(&event, 0, sizeof(event));
    event.type = ClientMessage;
    event.xclient.window = window;
    event.xclient.message_type = state;
    event.xclient.format = 32;
    event.xclient.data.l[0] = 1;
    event.xclient.data.l[1] = fullscreen;
    event.xclient.data.l[2] = 0;

    /* Set fullscreen */
    XMapWindow(*display, window);
    XSendEvent(*display, root, False, SubstructureRedirectMask | SubstructureNotifyMask, &event);
    XFlush(*display);

    return window;

#else
    *display = EGL_DEFAULT_DISPLAY;
    return (EGLNativeWindowType)0;
#endif /* X11BUILD */
}

void window_destroy(EGLNativeDisplayType display, EGLNativeWindowType window)
{
    DEBUG("window_destroy()");
#ifdef X11BUILD

    assert(display != NULL);
    assert(window != 0);
    XMapWindow(display, window);
    XDestroyWindow(display, window);
    XCloseDisplay(display);

#endif /* X11BUILD */
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
