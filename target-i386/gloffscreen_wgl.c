/*
 *  Offscreen OpenGL abstraction layer - WGL (windows) specific
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
#ifdef _WIN32
#include "gloffscreen.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <windows.h>
#include <GL/gl.h>
#include <GL/glext.h>
#include <GL/wglext.h>


struct GloMain {
  HINSTANCE             hInstance;
  HDC                   hDC;
  HWND                  hWnd;
  HGLRC                 hContext;
};
struct GloMain glo;
int glo_inited = 0;

struct _GloSurface {
  GLuint                width;
  GLuint                height;
  GLuint                formatFlags;

  HPBUFFERARB           hPBuffer;
  HDC                   hDC;
  HGLRC                 hRC;
};

#define GLO_WINDOW_CLASS "QEmuGLClass"
#define DEFAULT_DEPTH_BUFFER (16)

PFNWGLCHOOSEPIXELFORMATARBPROC wglChoosePixelFormatARB;
PFNWGLGETPBUFFERDCARBPROC wglGetPbufferDCARB;
PFNWGLRELEASEPBUFFERDCARBPROC wglReleasePbufferDCARB;
PFNWGLCREATEPBUFFERARBPROC wglCreatePbufferARB;
PFNWGLDESTROYPBUFFERARBPROC wglDestroyPbufferARB;
/* ------------------------------------------------------------------------ */

/* Initialise gloffscreen */
void glo_init(void) {
    WNDCLASSEX wcx;
    PIXELFORMATDESCRIPTOR pfd;

    if (glo_inited) {
        printf( "gloffscreen already inited\n" );
        exit( EXIT_FAILURE );
    }

    glo.hInstance = GetModuleHandle(NULL); // Grab An Instance For Our Window

    wcx.cbSize = sizeof(wcx);
    wcx.style = 0;
    wcx.lpfnWndProc = DefWindowProc;
    wcx.cbClsExtra = 0;
    wcx.cbWndExtra = 0;
    wcx.hInstance = glo.hInstance;
    wcx.hIcon = NULL;
    wcx.hCursor = NULL;
    wcx.hbrBackground = NULL;
    wcx.lpszMenuName =  NULL;
    wcx.lpszClassName = GLO_WINDOW_CLASS;
    wcx.hIconSm = NULL;
    RegisterClassEx(&wcx);
    glo.hWnd = CreateWindow(
        GLO_WINDOW_CLASS,
        "QEmuGL",
        0,0,0,0,0,
        (HWND)NULL, (HMENU)NULL,
        glo.hInstance,
        (LPVOID) NULL);

    if (!glo.hWnd) {
      printf( "Unable to create window\n" );
      exit( EXIT_FAILURE );
    }
    glo.hDC = GetDC(glo.hWnd);

    memset(&pfd, 0, sizeof(PIXELFORMATDESCRIPTOR));
    pfd.nSize = sizeof(PIXELFORMATDESCRIPTOR);
    pfd.nVersion = 1;
    pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL;
    pfd.iPixelType = PFD_TYPE_RGBA;
    pfd.cColorBits = 24;
    pfd.iLayerType = PFD_MAIN_PLANE;
    unsigned int pixelFormat = ChoosePixelFormat(glo.hDC, &pfd);
    DescribePixelFormat(glo.hDC, pixelFormat, sizeof(PIXELFORMATDESCRIPTOR), &pfd);
    if (!SetPixelFormat(glo.hDC, pixelFormat, &pfd))
        return;

    glo.hContext = wglCreateContext(glo.hDC);
    if (glo.hContext == NULL) {
      printf( "Unable to create GL context\n" );
      exit( EXIT_FAILURE );
    }
    wglMakeCurrent(glo.hDC, glo.hContext);

    // load in the extensions we need
    //const char	*ext = wglGetExtensionsStringARB(hdc);
    //"WGL_ARB_pixel_format" "WGL_ARB_pbuffer"

    wglChoosePixelFormatARB = (PFNWGLCHOOSEPIXELFORMATARBPROC)wglGetProcAddress("wglChoosePixelFormatARB");
    wglGetPbufferDCARB = (PFNWGLGETPBUFFERDCARBPROC)wglGetProcAddress("wglGetPbufferDCARB");
    wglReleasePbufferDCARB = (PFNWGLRELEASEPBUFFERDCARBPROC)wglGetProcAddress("wglReleasePbufferDCARB");
    wglCreatePbufferARB = (PFNWGLCREATEPBUFFERARBPROC)wglGetProcAddress("wglCreatePbufferARB");
    wglDestroyPbufferARB = (PFNWGLDESTROYPBUFFERARBPROC)wglGetProcAddress("wglDestroyPbufferARB");
    if (!wglChoosePixelFormatARB ||
        !wglGetPbufferDCARB ||
        !wglReleasePbufferDCARB ||
        !wglCreatePbufferARB ||
        !wglDestroyPbufferARB) {
      printf( "Unable to load the required WGL extensions\n" );
      exit( EXIT_FAILURE );
    }
}

/* Uninitialise gloffscreen */
void glo_kill(void) {
    if (glo.hContext) {
      wglMakeCurrent(NULL, NULL);
      wglDeleteContext(glo.hContext);
      glo.hContext = NULL;
    }
    if (glo.hDC) {
      ReleaseDC(glo.hWnd, glo.hDC);
      glo.hDC = NULL;
    }
    if (glo.hWnd) {
      DestroyWindow(glo.hWnd);
      glo.hWnd = NULL;
    }
    UnregisterClass(GLO_WINDOW_CLASS, glo.hInstance);
}

/* ------------------------------------------------------------------------ */

/* Create a surface with given width and height, formatflags are from the
 * GLO_ constants */
GloSurface *glo_surface_create(int width, int height, int formatFlags) {
    GloSurface           *surface;
    int                   rgbaBits[4];
    // pixel format attributes
    int pf_attri[] = {
       WGL_SUPPORT_OPENGL_ARB, TRUE,      
       WGL_DRAW_TO_PBUFFER_ARB, TRUE,     
       WGL_RED_BITS_ARB, 8,             
       WGL_GREEN_BITS_ARB, 8,            
       WGL_BLUE_BITS_ARB, 8,            
       WGL_ALPHA_BITS_ARB, 8,
       WGL_DEPTH_BITS_ARB, 0,
       WGL_STENCIL_BITS_ARB, 0,
       WGL_DOUBLE_BUFFER_ARB, FALSE,      
       0                                
    };
    float pf_attrf[] = {0, 0};
    unsigned int numReturned = 0;
    int          pixelFormat;
    int          pb_attr[] = { 0 }; // no specific pbuffer attributes

    if (!glo_inited)
      glo_init();

    // set up the surface format from the flags we were given
    glo_flags_get_rgba_bits(formatFlags, rgbaBits);
    pf_attri[5]  = rgbaBits[0];
    pf_attri[7]  = rgbaBits[1];
    pf_attri[9]  = rgbaBits[2];
    pf_attri[11] = rgbaBits[3];
    pf_attri[13] = glo_flags_get_depth_bits(formatFlags);
    pf_attri[15] = glo_flags_get_stencil_bits(formatFlags);

    wglChoosePixelFormatARB( glo.hDC, pf_attri, pf_attrf, 1, &pixelFormat, &numReturned);
    if( numReturned == 0 ) {
        printf( "No matching configs found.\n" );
        exit( EXIT_FAILURE );
    }

    // Create the p-buffer...
    surface = (GloSurface*)malloc(sizeof(GloSurface));
    memset(surface, 0, sizeof(GloSurface));
    surface->width = width;
    surface->height = height;
    surface->formatFlags = formatFlags;
    surface->hPBuffer = wglCreatePbufferARB( glo.hDC, pixelFormat, surface->width, surface->height, pb_attr );
    surface->hDC      = wglGetPbufferDCARB( surface->hPBuffer );
    surface->hRC      = wglCreateContext( surface->hDC );

    if( !surface->hPBuffer ) {
      printf( "Couldn't create the PBuffer\n" );
      exit( EXIT_FAILURE );
    }


    glo_surface_makecurrent(surface);

    return surface;
}

/* Destroy the given surface */
void glo_surface_destroy(GloSurface *surface) {
    if( surface->hRC != NULL ) {
        wglMakeCurrent( surface->hDC, surface->hRC );
//       wglDeleteContext( surface->hRC ); // why not?
        wglReleasePbufferDCARB( surface->hPBuffer, surface->hDC );
        wglDestroyPbufferARB( surface->hPBuffer );
        surface->hRC = NULL;
    }
    if( surface->hDC != NULL ) {
        ReleaseDC( glo.hWnd, surface->hDC );
        surface->hDC = NULL;
    }
    free(surface);
}

/* Make the given surface current */
void glo_surface_makecurrent(GloSurface *surface) {
    wglMakeCurrent( surface->hDC, surface->hRC );
}

/* Get the contents of the given surface */
void glo_surface_getcontents(GloSurface *surface, void *data) {
    int bpp = glo_flags_get_bytes_per_pixel(surface->formatFlags);
    int rowsize = surface->width*bpp;
    int glFormat, glType;
    glo_flags_get_readpixel_type(surface->formatFlags, &glFormat, &glType);
#ifdef GETCONTENTS_INDIVIDUAL
    GLubyte *b = (GLubyte *)data;
    int irow;
    for(irow = surface->height-1 ; irow >= 0 ; irow--) {
        glReadPixels(0, irow, surface->width, 1, glFormat, glType, b);
        b += rowsize;
    }
#else
    // unflipped
    GLubyte *b = (GLubyte *)data;
    GLubyte *c = &((GLubyte *)data)[rowsize*(surface->height-1)];
    GLubyte *tmp = (GLubyte*)malloc(rowsize);
    int irow;
    glReadPixels(0, 0, surface->width, surface->height, glFormat, glType, data);
    for(irow = 0; irow < surface->height/2; irow++) {
        memcpy(tmp, b, rowsize);
        memcpy(b, c, rowsize);
        memcpy(c, tmp, rowsize);
        b += rowsize;
        c -= rowsize;
    }
    free(tmp);
#endif
}

/* Return the width and height of the given surface */
void glo_surface_get_size(GloSurface *surface, int *width, int *height) {
    if (width)
      *width = surface->width;
    if (height)
      *height = surface->height;
}
#endif
