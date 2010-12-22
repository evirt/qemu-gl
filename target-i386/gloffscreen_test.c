/*
 * main.c
 *
 *  Created on: 5 May 2010
 *  Author: gw
 */

#include "gloffscreen.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#include <GL/gl.h>
#include <GL/glext.h>
#else
#include <GL/gl.h>
#include <sys/time.h>
#endif

// ---------------------------------------------------
//  Copied from glx.h as we need them in windows too
/*
 * Tokens for glXChooseVisual and glXGetConfig:
 */
#define GLX_USE_GL      1
#define GLX_BUFFER_SIZE     2
#define GLX_LEVEL       3
#define GLX_RGBA        4
#define GLX_DOUBLEBUFFER    5
#define GLX_STEREO      6
#define GLX_AUX_BUFFERS     7
#define GLX_RED_SIZE        8
#define GLX_GREEN_SIZE      9
#define GLX_BLUE_SIZE       10
#define GLX_ALPHA_SIZE      11
#define GLX_DEPTH_SIZE      12
#define GLX_STENCIL_SIZE    13
#define GLX_ACCUM_RED_SIZE  14
#define GLX_ACCUM_GREEN_SIZE    15
#define GLX_ACCUM_BLUE_SIZE 16
#define GLX_ACCUM_ALPHA_SIZE    17
// ---------------------------------------------------

#define TX (32)
#define TY (32)

int main() {
    int test_failure = 0;
    GloContext *context;
    GloSurface *surface;
    unsigned char *datain = (unsigned char *)malloc(4*TX*TY);
    unsigned char *datain_flip = (unsigned char *)malloc(4*TX*TY); // flipped input data (for GL)
    unsigned char *dataout = (unsigned char *)malloc(4*TX*TY);
    unsigned char *p;
    int x,y;
    unsigned int bufferAttributes[] = {
            GLX_RED_SIZE,      8,
            GLX_GREEN_SIZE,    8,
            GLX_BLUE_SIZE,     8,
            GLX_ALPHA_SIZE,    8,
            GLX_DEPTH_SIZE,    0,
            GLX_STENCIL_SIZE,  0,
            0,
        };
    int bufferFlags = glo_flags_get_from_glx(bufferAttributes, 0);
    int bpp = glo_flags_get_bytes_per_pixel(bufferFlags);
    int glFormat, glType;
    memset(datain_flip, 0, TX*TY*4);
    memset(datain, 0, TX*TY*4);
    p = datain;
    for (y=0;y<TY;y++) {
      for (x=0;x<TX;x++) {
        p[0] = x;
        p[1] = y;
        //if (y&1) { p[0]=0; p[1]=0; }
        if (bpp>2) p[2] = 0;
        if (bpp>3) p[3] = 0xFF;
        p+=bpp;
      }
      memcpy(&datain_flip[((TY-1)-y)*bpp*TX], &datain[y*bpp*TX], bpp*TX);
    }

    glo_init();
    // new surface
    context = glo_context_create(bufferFlags, 0);
    surface = glo_surface_create(TX, TY, context);
    glo_surface_makecurrent(surface);
    //printf("GL VENDOR %s\n", glGetString(GL_VENDOR));
    //printf("GL RENDERER %s\n", glGetString(GL_RENDERER));
    //printf("GL VERSION %s\n", glGetString(GL_VERSION));

    if (strstr (glGetString(GL_RENDERER), "Software")) {
        //printf ("Host does not have GL hardware acceleration!\n");
        test_failure = 1;
        goto TEST_END;
    }

    // fill with stuff (in correctly ordered way)
    glClear(GL_COLOR_BUFFER_BIT);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0,TX, 0,TY, 0, 1);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glRasterPos2f(0,0);
    glo_flags_get_readpixel_type(bufferFlags, &glFormat, &glType);
    // printf("glFormat: 0x%08X glType: 0x%08X\n", glFormat, glType);
    glDrawPixels(TX,TY,glFormat, glType, datain_flip);
    glFlush();

    memset(dataout, 0, bpp*TX*TY);

    glo_surface_getcontents(surface, TX*bpp, bpp*8, dataout);

    // destroy surface
    glo_surface_destroy(surface);
    glo_context_destroy(context);
    // compare
    if (memcmp(datain, dataout, bpp*TX*TY)!=0) {
      test_failure = 1;
    }
TEST_END:
    glo_kill();
    //printf ("Testing %s\n", (test_failure ? "FAILED" : "PASSED"));
    return test_failure;
}
