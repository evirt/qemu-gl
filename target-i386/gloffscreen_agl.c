/*
 *  Offscreen OpenGL abstraction layer - AGL specific
 *
 *  Copyright (c) 2010 Nokia Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */
#include "config-host.h"
#ifdef CONFIG_COCOA
#include "gloffscreen.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

// workaround double definition of uint16 type
#define __SECURITYHI__

#include <AGL/agl.h>
#include <dlfcn.h>

struct _GloContext {
    GLuint         formatFlags;

    AGLPixelFormat pf;
    AGLContext     context;
};

struct _GloSurface {
    GLuint         width;
    GLuint         height;

    GloContext     *context;
    AGLPbuffer     pbuffer;
};

extern void glo_surface_getcontents_readpixels(int formatFlags, int stride,
                                               int bpp, int width, int height,
                                               void *data);

/* ------------------------------------------------------------------------ */

int glo_initialised(void) {
    return 1;
}

/* Initialise gloffscreen */
void glo_init(void) {
}

/* Uninitialise gloffscreen */
void glo_kill(void) {
}

const char *glo_glXQueryExtensionsString(void) {
    return "";
}

/* Like wglGetProcAddress/glxGetProcAddress */
void *glo_getprocaddress(const char *procName) {
    if (!strncmp(procName, "glX", 3)) {
        return (void *)1;
    }
    return dlsym(RTLD_NEXT, procName);
}

/* ------------------------------------------------------------------------ */

/* Create an OpenGL context for a certain pixel format.
   formatflags are from the GLO_ constants */

GloContext *glo_context_create(int formatFlags, GloContext *shareLists) {
    // set up the surface format from the flags we were given
    int rgbaBits[4];
    glo_flags_get_rgba_bits(formatFlags, rgbaBits);
    const GLint bufferAttributes[] = {
        AGL_PBUFFER,
        AGL_RGBA,
        AGL_MINIMUM_POLICY,
        AGL_ACCELERATED,
        AGL_RED_SIZE, rgbaBits[0],
        AGL_GREEN_SIZE, rgbaBits[1],
        AGL_BLUE_SIZE, rgbaBits[2],
        AGL_ALPHA_SIZE, rgbaBits[3],
        AGL_DEPTH_SIZE, glo_flags_get_depth_bits(formatFlags),
        AGL_STENCIL_SIZE, glo_flags_get_stencil_bits(formatFlags),
        AGL_NONE
    };
    AGLPixelFormat pf = aglChoosePixelFormat(NULL, 0, bufferAttributes);
    if (pf == NULL) {
        printf("No matching configs found.\n");
        exit(EXIT_FAILURE);
    }
    GloContext *context = qemu_mallocz(sizeof(*context));
    context->formatFlags = formatFlags;
    context->pf = pf;
    context->context = aglCreateContext(context->pf, 
                                        shareLists ? shareLists->context
                                                   : NULL);
    if (!context->context) {
        printf("aglCreateContext failed\n");
        exit(EXIT_FAILURE);
    }
    return context;
}

/* Destroy a previously created OpenGL context */
void glo_context_destroy(GloContext *context) {
    if (context) {
        // TODO: check for GloSurfaces using this?
        aglDestroyContext(context->context);
        aglDestroyPixelFormat(context->pf);
        qemu_free(context);
    }
}

/* ------------------------------------------------------------------------ */

/* Create a surface with given width and height, formatflags are from the
 * GLO_ constants */
GloSurface *glo_surface_create(int width, int height, GloContext *context) {
    GloSurface *surface = qemu_mallocz(sizeof(*surface));
    surface->width = width;
    surface->height = height;
    surface->context = context;
    if (aglCreatePBuffer(surface->width, surface->height,
                         GL_TEXTURE_RECTANGLE_EXT, GL_RGBA, 0,
                         &surface->pbuffer) == GL_FALSE) {
        printf("Couldn't create PBuffer\n");
        exit(EXIT_FAILURE);
    }
    return surface;
}

/* Destroy the given surface */
void glo_surface_destroy(GloSurface *surface) {
    if (surface) {
        aglDestroyPBuffer(surface->pbuffer);
        qemu_free(surface);
    }
}

/* Make the given surface current */
int glo_surface_makecurrent(GloSurface *surface) {
    return surface ? aglSetPBuffer(surface->context->context, surface->pbuffer,
                                   0, 0, 0)
                   : GL_TRUE;
}

/* Get the contents of the given surface */
void glo_surface_getcontents(GloSurface *surface, int stride, int bpp,
                             void *data) {
    if (surface) {
        // Compatible / fallback method.
        glo_surface_getcontents_readpixels(surface->context->formatFlags,
                                           stride, bpp, surface->width,
                                           surface->height, data);
    }
}

/* Return the width and height of the given surface */
void glo_surface_get_size(GloSurface *surface, int *width, int *height) {
    if (surface) {
       if (width) {
            *width = surface->width;
        }
        if (height) {
            *height = surface->height;
        }
    }
}

#endif
