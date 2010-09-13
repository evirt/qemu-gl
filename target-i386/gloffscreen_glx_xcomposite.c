/*
 *  Offscreen OpenGL abstraction layer - GLX specific
 *
 *  Copyright (c) 2010 Intel Corporation
 *  Written by: 
 *    Gordon Williams <gordon.williams@collabora.co.uk>
 *    Ian Molton <ian.molton@collabora.co.uk>
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

#include <sys/shm.h>
#include <X11/extensions/Xcomposite.h>
#include <X11/extensions/XShm.h>
#include "gloffscreen_glx.h"

extern void *qemu_malloc(size_t size);
extern void *qemu_realloc(void *ptr, size_t size);
extern void qemu_free(void *ptr);

extern struct GloMain glo;

struct _GloDrawable {
    Window         xWindow;
};

GLXFBConfig glo_get_fbconfig (int formatFlags);
void glo_surface_drawable_create (GloSurface *surface);
void glo_surface_drawable_destroy (GloSurface *surface);
int glo_surface_makecurrent_internal (GloSurface *surface);

GLXFBConfig glo_get_fbconfig (int formatFlags)
{
  GLXFBConfig          *fbConfigs;
  int                   numReturned;
  int                   rgbaBits[4];
  int                   bufferAttributes[] = {
      GLX_DRAWABLE_TYPE, GLX_WINDOW_BIT,
      GLX_RENDER_TYPE,   GLX_RGBA_BIT,
      GLX_RED_SIZE,      8,
      GLX_GREEN_SIZE,    8,
      GLX_BLUE_SIZE,     8,
      GLX_ALPHA_SIZE,    8,
      GLX_DEPTH_SIZE,    0,
      GLX_STENCIL_SIZE,  0,
      None
  };

  // set up the surface format from the flags we were given
  glo_flags_get_rgba_bits(formatFlags, rgbaBits);
  bufferAttributes[5]  = rgbaBits[0];
  bufferAttributes[7]  = rgbaBits[1];
  bufferAttributes[9]  = rgbaBits[2];
  bufferAttributes[11] = rgbaBits[3];
  bufferAttributes[13] = glo_flags_get_depth_bits(formatFlags);
  bufferAttributes[15] = glo_flags_get_stencil_bits(formatFlags);

  //printf("Got R%d, G%d, B%d, A%d\n", rgbaBits[0], rgbaBits[1], rgbaBits[2], rgbaBits[3]);

  fbConfigs = glXChooseFBConfig( glo.dpy, DefaultScreen(glo.dpy),
                                 bufferAttributes, &numReturned );
  if (numReturned==0) {
      printf( "No matching configs found.\n" );
      exit( EXIT_FAILURE );
  }
  return fbConfigs[0];
}

void glo_surface_drawable_create(GloSurface *surface)
{
    XVisualInfo *vis;
    XSetWindowAttributes attr = { 0 };
    unsigned long mask = 0;

    vis = glXGetVisualFromFBConfig(glo.dpy, surface->context->fbConfig);
    /* window attributes */
    attr.background_pixel = 0xff000000;
    attr.border_pixel = 0;
    attr.colormap = XCreateColormap(glo.dpy, DefaultRootWindow (glo.dpy), vis->visual, AllocNone);
    attr.event_mask = 0;
    attr.save_under = True;
    attr.override_redirect = True;
    attr.cursor = None;

    mask =
        CWBackPixel | CWBorderPixel | CWColormap | CWEventMask |
        CWOverrideRedirect | CWSaveUnder;

    surface->offscreen_drawable = qemu_malloc (sizeof (struct _GloDrawable));
    surface->offscreen_drawable->xWindow = XCreateWindow (glo.dpy, DefaultRootWindow (glo.dpy), 
                                                         -surface->width, -surface->height, 
                                                         surface->width, surface->height, 
                                                         0, vis->depth, 
                                                         InputOutput, vis->visual, mask, &attr);
    if (!surface->offscreen_drawable->xWindow) {
      printf( "XCreateWindow failed\n" );
      exit( EXIT_FAILURE );
    }

    XCompositeRedirectWindow (glo.dpy, surface->offscreen_drawable->xWindow, 
                              CompositeRedirectManual);
    XMapWindow(glo.dpy, surface->offscreen_drawable->xWindow);
    surface->xPixmap = XCompositeNameWindowPixmap(glo.dpy, surface->offscreen_drawable->xWindow);
    if(surface->xPixmap == 0) {
      printf( "XCreatePixmap failed\n" );
      exit( EXIT_FAILURE );
    }
}

/* Destroy the given surface */
void glo_surface_drawable_destroy (GloSurface *surface)
{
    XDestroyWindow (glo.dpy, surface->offscreen_drawable->xWindow);
    qemu_free (surface->offscreen_drawable);
    XFreePixmap( glo.dpy, surface->xPixmap);
}

int glo_surface_makecurrent_internal (GloSurface *surface)
{
    return glXMakeCurrent (glo.dpy, surface->offscreen_drawable->xWindow,
                           surface->context->context);
}
#endif /*_WIN32*/
