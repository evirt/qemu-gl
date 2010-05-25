/*
 *  Offscreen OpenGL abstraction layer - GLX specific
 *
 *  Copyright (c) 2010 Gordon Williams <gordon.williams@collabora.co.uk>
 *  Copyright (c) 2010 Ian Molton <ian.molton@collabora.co.uk>
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
#ifndef _WIN32
#include "gloffscreen.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <GL/gl.h>
#include <GL/glx.h>

struct GloMain {
  Display *dpy;
};
struct GloMain glo;
int glo_inited = 0;

struct _GloSurface {
  GLuint                width;
  GLuint                height;
  GLuint                formatFlags;

  Pixmap                xPixmap;
  GLXContext            context;
  GLXPixmap             glxPixmap;
};

/* ------------------------------------------------------------------------ */

/* Initialise gloffscreen */
void glo_init(void) {
    if (glo_inited) {
        printf( "gloffscreen already inited\n" );
        exit( EXIT_FAILURE );
    }
    /* Open a connection to the X server */
    glo.dpy = XOpenDisplay( NULL );
    if ( glo.dpy == NULL ) {
        printf( "Unable to open a connection to the X server\n" );
        exit( EXIT_FAILURE );
    }
    glo_inited = 1;
}

/* Uninitialise gloffscreen */
void glo_kill(void) {
    XCloseDisplay(glo.dpy);
    glo.dpy = NULL;
}

/* ------------------------------------------------------------------------ */

/* Create a surface with given width and height, formatflags are from the
 * GLO_ constants */
GloSurface *glo_surface_create(int width, int height, int formatFlags, GloSurface *shareWith) {
    GLXFBConfig          *fbConfigs;
    int                   numReturned;
    GloSurface           *surface;
    int                   rgbaBits[4];
    GLXContext            shareLists = NULL;
    int bufferDepth;
    int bufferAttributes[] = {
        GLX_DRAWABLE_TYPE, GLX_WINDOW_BIT,
        GLX_RENDER_TYPE,   GLX_RGBA_BIT,
        GLX_RED_SIZE,      8,
        GLX_GREEN_SIZE,    8,
        GLX_BLUE_SIZE,     8,
        GLX_ALPHA_SIZE,    8,
        GLX_DEPTH_SIZE,    0,
        GLX_STENCIL_SIZE,  0,
        //GLX_DOUBLEBUFFER,  1,
        None
    };

    if (!glo_inited)
      glo_init();
    // set up the surface format from the flags we were given
    glo_flags_get_rgba_bits(formatFlags, rgbaBits);
    bufferAttributes[5]  = rgbaBits[0];
    bufferAttributes[7]  = rgbaBits[1];
    bufferAttributes[9]  = rgbaBits[2];
    bufferAttributes[11] = rgbaBits[3];
    bufferDepth = rgbaBits[0]+rgbaBits[1]+rgbaBits[2]+rgbaBits[3];
    bufferAttributes[13] = glo_flags_get_depth_bits(formatFlags);
    bufferAttributes[15] = glo_flags_get_stencil_bits(formatFlags);

    printf("Got R%d, G%d, B%d, A%d\n", rgbaBits[0], rgbaBits[1], rgbaBits[2], rgbaBits[3]);

    fbConfigs = glXChooseFBConfig( glo.dpy, DefaultScreen(glo.dpy),
                                   bufferAttributes, &numReturned );
    if (numReturned==0) {
        printf( "No matching configs found.\n" );
        exit( EXIT_FAILURE );
    }
    surface = (GloSurface*)malloc(sizeof(GloSurface));
    memset(surface, 0, sizeof(GloSurface));
    surface->width = width;
    surface->height = height;
    surface->formatFlags = formatFlags;
    surface->xPixmap = XCreatePixmap( glo.dpy, DefaultRootWindow(glo.dpy),
                                      width, height, bufferDepth);
    if (!surface->xPixmap) {
      printf( "XCreatePixmap failed\n" );
      exit( EXIT_FAILURE );
    }

    if (shareWith)
      shareLists = shareWith->context;

    /* Create a GLX context for OpenGL rendering */
    surface->context = glXCreateNewContext( glo.dpy, fbConfigs[0],
                                            GLX_RGBA_TYPE, shareLists, True );
    if (!surface->context) {
      printf( "glXCreateNewContext failed\n" );
      exit( EXIT_FAILURE );
    }

    if (shareLists)
      glXCopyContext(glo.dpy, shareLists, surface->context, GL_ALL_ATTRIB_BITS);

    if (shareLists)
      glXCopyContext(glo.dpy, shareLists, surface->context, GL_ALL_ATTRIB_BITS);

    /* Create a GLX window to associate the frame buffer configuration
    ** with the created X window */
    surface->glxPixmap = glXCreatePixmap( glo.dpy, fbConfigs[0], surface->xPixmap, NULL );
    if (!surface->glxPixmap) {
      printf( "glXCreatePixmap failed\n" );
      exit( EXIT_FAILURE );
    }

    return surface;
}

/* Destroy the given surface */
void glo_surface_destroy(GloSurface *surface) {
    glXDestroyPixmap( glo.dpy, surface->glxPixmap);
    XFreePixmap( glo.dpy, surface->xPixmap);
    glXDestroyContext( glo.dpy, surface->context);
    free(surface);
}

/* Make the given surface current */
int glo_surface_makecurrent(GloSurface *surface) {
    int ret;

    if (!glo_inited)
      glo_init();

    if (surface) {
      ret = glXMakeCurrent(glo.dpy, surface->glxPixmap, surface->context);
    } else {
      ret = glXMakeCurrent(glo.dpy, 0, NULL);
    }

    return ret;
}

/* Get the contents of the given surface */
void glo_surface_getcontents(GloSurface *surface, int stride, void *data) {
    if (!surface) return;
//#define GETCONTENTS_INDIVIDUAL true
#if 1
    // 2593us/frame for 256x256 (individual readpixels)
    // 161us/frame for 256x256 (unflipped)
    // 314us/frame for 256x256 (single readpixels + software flip)
//    int bpp = glo_flags_get_bytes_per_pixel(surface->formatFlags);
//    int rowsize = surface->width*bpp;
    int glFormat, glType;
    glo_flags_get_readpixel_type(surface->formatFlags, &glFormat, &glType);
#ifdef GETCONTENTS_INDIVIDUAL
    GLubyte *b = (GLubyte *)data;
    int irow;
    for(irow = surface->height-1 ; irow >= 0 ; irow--) {
        glReadPixels(0, irow, surface->width, 1, glFormat, glType, b);
        b += stride;
    }
#else
    // unflipped
    GLubyte *b = (GLubyte *)data;
    GLubyte *c = &((GLubyte *)data)[stride*(surface->height-1)];
    GLubyte *tmp = (GLubyte*)malloc(stride);
    int irow;
    //printf("DP 0x%08X 0x%08X\n", glFormat, glType);
    glPixelStorei(GL_PACK_ALIGNMENT, 4);//FIXMEIM - calculate ?
    glReadPixels(0, 0, surface->width, surface->height, glFormat, glType, data);
    for(irow = 0; irow < surface->height/2; irow++) {
        memcpy(tmp, b, stride);
        memcpy(b, c, stride);
        memcpy(c, tmp, stride);
        b += stride;
        c -= stride;
    }
//    free(tmp);
//    {
//        FILE *d = fopen("dbg.rgb", "wb");
//	fwrite(b, surface->width*4, surface->height, d);
//	fclose(d);
//    }
#endif
#else
   // glXSwapBuffers(glo.dpy, surface->glxPixmap);
    XImage *img =
        XGetImage(glo.dpy, surface->xPixmap, 0, 0, surface->width, surface->height, AllPlanes, ZPixmap);
    if (img) {
      int bpp = glo_flags_get_bytes_per_pixel(surface->formatFlags);
      memcpy(data, img->data, bpp*surface->width*surface->height);
      printf("BPL %d, bpp %d, depth %d 0x%08X 0x%08X 0x%08X\n", img->bytes_per_line, img->bits_per_pixel, img->depth, img->red_mask, img->green_mask, img->blue_mask);
      img->f.destroy_image(img);
    } else
      printf("XGetImage fail\n");
#endif
}

/* Return the width and height of the given surface */
void glo_surface_get_size(GloSurface *surface, int *width, int *height) {
    if (width)
      *width = surface->width;
    if (height)
      *height = surface->height;
}

Display *glo_get_dpy(void) {
  if (!glo_inited)
        glo_init();
  return glo.dpy;
}
#endif
