/*
 *  Copyright (c) 2006,2007 Even Rouault
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

#ifndef INCLUDE_OPENGL_FUNC_H
#define INCLUDE_OPENGL_FUNC_H

#include "mesa_gl.h"
#include "mesa_glext.h"

#define NEED_CPU_H
#include "exec.h"  // For arg_t typedef
#undef NEED_CPU_H

/* Argument list are internally of a type that can hold a target pointer
 * or a host pointer.  */
typedef target_phys_addr_t arg_t;

enum {
    TYPE_NONE,
    TYPE_CHAR,
    TYPE_UNSIGNED_CHAR,
    TYPE_SHORT,
    TYPE_UNSIGNED_SHORT,
    TYPE_INT,
    TYPE_UNSIGNED_INT,
    TYPE_FLOAT,
    TYPE_DOUBLE,
    TYPE_1CHAR,
    TYPE_2CHAR,
    TYPE_3CHAR,
    TYPE_4CHAR,
    TYPE_128UCHAR,
    TYPE_1SHORT,
    TYPE_2SHORT,
    TYPE_3SHORT,
    TYPE_4SHORT,
    TYPE_1INT,
    TYPE_2INT,
    TYPE_3INT,
    TYPE_4INT,
    TYPE_1FLOAT,
    TYPE_2FLOAT,
    TYPE_3FLOAT,
    TYPE_4FLOAT,
    TYPE_16FLOAT,
    TYPE_1DOUBLE,
    TYPE_2DOUBLE,
    TYPE_3DOUBLE,
    TYPE_4DOUBLE,
    TYPE_16DOUBLE,
    TYPE_OUT_1INT,
    TYPE_OUT_1FLOAT,
    TYPE_OUT_4CHAR,
    TYPE_OUT_4INT,
    TYPE_OUT_4FLOAT,
    TYPE_OUT_4DOUBLE,
    TYPE_OUT_128UCHAR,
    TYPE_CONST_CHAR,
    TYPE_ARRAY_CHAR,
    TYPE_ARRAY_SHORT,
    TYPE_ARRAY_INT,
    TYPE_ARRAY_FLOAT,
    TYPE_ARRAY_DOUBLE,
    TYPE_IN_IGNORED_POINTER,
    TYPE_OUT_ARRAY_CHAR,
    TYPE_OUT_ARRAY_SHORT,
    TYPE_OUT_ARRAY_INT,
    TYPE_OUT_ARRAY_FLOAT,
    TYPE_OUT_ARRAY_DOUBLE,
    TYPE_NULL_TERMINATED_STRING,

    TYPE_ARRAY_CHAR_OF_LENGTH_DEPENDING_ON_PREVIOUS_ARGS,
    TYPE_ARRAY_SHORT_OF_LENGTH_DEPENDING_ON_PREVIOUS_ARGS,
    TYPE_ARRAY_INT_OF_LENGTH_DEPENDING_ON_PREVIOUS_ARGS,
    TYPE_ARRAY_FLOAT_OF_LENGTH_DEPENDING_ON_PREVIOUS_ARGS,
    TYPE_ARRAY_DOUBLE_OF_LENGTH_DEPENDING_ON_PREVIOUS_ARGS,
    TYPE_OUT_ARRAY_CHAR_OF_LENGTH_DEPENDING_ON_PREVIOUS_ARGS,
    TYPE_OUT_ARRAY_SHORT_OF_LENGTH_DEPENDING_ON_PREVIOUS_ARGS,
    TYPE_OUT_ARRAY_INT_OF_LENGTH_DEPENDING_ON_PREVIOUS_ARGS,
    TYPE_OUT_ARRAY_FLOAT_OF_LENGTH_DEPENDING_ON_PREVIOUS_ARGS,
    TYPE_OUT_ARRAY_DOUBLE_OF_LENGTH_DEPENDING_ON_PREVIOUS_ARGS,
    /* .... */
    TYPE_LAST,
    /* .... */
    TYPE_1UCHAR = TYPE_CHAR,
    TYPE_1USHORT = TYPE_1SHORT,
    TYPE_1UINT = TYPE_1INT,
    TYPE_OUT_1UINT = TYPE_OUT_1INT,
    TYPE_OUT_4UCHAR = TYPE_OUT_4CHAR,
    TYPE_ARRAY_VOID = TYPE_ARRAY_CHAR,
    TYPE_ARRAY_SIGNED_CHAR = TYPE_ARRAY_CHAR,
    TYPE_ARRAY_UNSIGNED_CHAR = TYPE_ARRAY_CHAR,
    TYPE_ARRAY_UNSIGNED_SHORT = TYPE_ARRAY_SHORT,
    TYPE_ARRAY_UNSIGNED_INT = TYPE_ARRAY_INT,
    TYPE_OUT_ARRAY_VOID = TYPE_OUT_ARRAY_CHAR,
    TYPE_OUT_ARRAY_SIGNED_CHAR = TYPE_OUT_ARRAY_CHAR,
    TYPE_OUT_ARRAY_UNSIGNED_CHAR = TYPE_OUT_ARRAY_CHAR,
    TYPE_OUT_ARRAY_UNSIGNED_SHORT = TYPE_OUT_ARRAY_SHORT,
    TYPE_OUT_ARRAY_UNSIGNED_INT = TYPE_OUT_ARRAY_INT,
    TYPE_ARRAY_VOID_OF_LENGTH_DEPENDING_ON_PREVIOUS_ARGS =
        TYPE_ARRAY_CHAR_OF_LENGTH_DEPENDING_ON_PREVIOUS_ARGS,
    TYPE_ARRAY_SIGNED_CHAR_OF_LENGTH_DEPENDING_ON_PREVIOUS_ARGS =
        TYPE_ARRAY_CHAR_OF_LENGTH_DEPENDING_ON_PREVIOUS_ARGS,
    TYPE_ARRAY_UNSIGNED_CHAR_OF_LENGTH_DEPENDING_ON_PREVIOUS_ARGS =
        TYPE_ARRAY_CHAR_OF_LENGTH_DEPENDING_ON_PREVIOUS_ARGS,
    TYPE_ARRAY_UNSIGNED_SHORT_OF_LENGTH_DEPENDING_ON_PREVIOUS_ARGS =
        TYPE_ARRAY_SHORT_OF_LENGTH_DEPENDING_ON_PREVIOUS_ARGS,
    TYPE_ARRAY_UNSIGNED_INT_OF_LENGTH_DEPENDING_ON_PREVIOUS_ARGS =
        TYPE_ARRAY_INT_OF_LENGTH_DEPENDING_ON_PREVIOUS_ARGS,
    TYPE_OUT_ARRAY_VOID_OF_LENGTH_DEPENDING_ON_PREVIOUS_ARGS =
        TYPE_OUT_ARRAY_CHAR_OF_LENGTH_DEPENDING_ON_PREVIOUS_ARGS,
    TYPE_OUT_ARRAY_SIGNED_CHAR_OF_LENGTH_DEPENDING_ON_PREVIOUS_ARGS =
        TYPE_OUT_ARRAY_CHAR_OF_LENGTH_DEPENDING_ON_PREVIOUS_ARGS,
    TYPE_OUT_ARRAY_UNSIGNED_CHAR_OF_LENGTH_DEPENDING_ON_PREVIOUS_ARGS =
        TYPE_OUT_ARRAY_CHAR_OF_LENGTH_DEPENDING_ON_PREVIOUS_ARGS,
    TYPE_OUT_ARRAY_UNSIGNED_SHORT_OF_LENGTH_DEPENDING_ON_PREVIOUS_ARGS =
        TYPE_OUT_ARRAY_SHORT_OF_LENGTH_DEPENDING_ON_PREVIOUS_ARGS,
    TYPE_OUT_ARRAY_UNSIGNED_INT_OF_LENGTH_DEPENDING_ON_PREVIOUS_ARGS =
        TYPE_OUT_ARRAY_INT_OF_LENGTH_DEPENDING_ON_PREVIOUS_ARGS,
};

#define CASE_IN_LENGTH_DEPENDING_ON_PREVIOUS_ARGS \
  case TYPE_ARRAY_CHAR_OF_LENGTH_DEPENDING_ON_PREVIOUS_ARGS: \
  case TYPE_ARRAY_SHORT_OF_LENGTH_DEPENDING_ON_PREVIOUS_ARGS: \
  case TYPE_ARRAY_INT_OF_LENGTH_DEPENDING_ON_PREVIOUS_ARGS: \
  case TYPE_ARRAY_FLOAT_OF_LENGTH_DEPENDING_ON_PREVIOUS_ARGS: \
  case TYPE_ARRAY_DOUBLE_OF_LENGTH_DEPENDING_ON_PREVIOUS_ARGS

#define CASE_OUT_LENGTH_DEPENDING_ON_PREVIOUS_ARGS \
  case TYPE_OUT_ARRAY_CHAR_OF_LENGTH_DEPENDING_ON_PREVIOUS_ARGS: \
  case TYPE_OUT_ARRAY_SHORT_OF_LENGTH_DEPENDING_ON_PREVIOUS_ARGS: \
  case TYPE_OUT_ARRAY_INT_OF_LENGTH_DEPENDING_ON_PREVIOUS_ARGS: \
  case TYPE_OUT_ARRAY_FLOAT_OF_LENGTH_DEPENDING_ON_PREVIOUS_ARGS: \
  case TYPE_OUT_ARRAY_DOUBLE_OF_LENGTH_DEPENDING_ON_PREVIOUS_ARGS

#define CASE_IN_UNKNOWN_SIZE_POINTERS \
  case TYPE_ARRAY_CHAR: \
  case TYPE_ARRAY_SHORT: \
  case TYPE_ARRAY_INT: \
  case TYPE_ARRAY_FLOAT: \
  case TYPE_ARRAY_DOUBLE

#define CASE_IN_KNOWN_SIZE_POINTERS \
  case TYPE_1CHAR:\
  case TYPE_2CHAR:\
  case TYPE_3CHAR:\
  case TYPE_4CHAR:\
  case TYPE_128UCHAR:\
  case TYPE_1SHORT:\
  case TYPE_2SHORT:\
  case TYPE_3SHORT:\
  case TYPE_4SHORT:\
  case TYPE_1INT:\
  case TYPE_2INT:\
  case TYPE_3INT:\
  case TYPE_4INT:\
  case TYPE_1FLOAT:\
  case TYPE_2FLOAT:\
  case TYPE_3FLOAT:\
  case TYPE_4FLOAT:\
  case TYPE_16FLOAT:\
  case TYPE_1DOUBLE:\
  case TYPE_2DOUBLE:\
  case TYPE_3DOUBLE:\
  case TYPE_4DOUBLE:\
  case TYPE_16DOUBLE

#define CASE_OUT_UNKNOWN_SIZE_POINTERS \
  case TYPE_OUT_ARRAY_CHAR: \
  case TYPE_OUT_ARRAY_SHORT: \
  case TYPE_OUT_ARRAY_INT: \
  case TYPE_OUT_ARRAY_FLOAT: \
  case TYPE_OUT_ARRAY_DOUBLE

#define CASE_OUT_KNOWN_SIZE_POINTERS \
  case TYPE_OUT_1INT: \
  case TYPE_OUT_1FLOAT: \
  case TYPE_OUT_4CHAR: \
  case TYPE_OUT_4INT: \
  case TYPE_OUT_4FLOAT: \
  case TYPE_OUT_4DOUBLE: \
  case TYPE_OUT_128UCHAR \

#define CASE_IN_POINTERS CASE_IN_UNKNOWN_SIZE_POINTERS: CASE_IN_KNOWN_SIZE_POINTERS: CASE_IN_LENGTH_DEPENDING_ON_PREVIOUS_ARGS
#define CASE_OUT_POINTERS CASE_OUT_UNKNOWN_SIZE_POINTERS: CASE_OUT_KNOWN_SIZE_POINTERS: CASE_OUT_LENGTH_DEPENDING_ON_PREVIOUS_ARGS

#define CASE_POINTERS CASE_IN_POINTERS: CASE_OUT_POINTERS
#define CASE_KNOWN_SIZE_POINTERS CASE_IN_KNOWN_SIZE_POINTERS: CASE_OUT_KNOWN_SIZE_POINTERS


#define IS_ARRAY_CHAR(type)  (type == TYPE_ARRAY_CHAR || type == TYPE_1CHAR || type == TYPE_2CHAR || type == TYPE_3CHAR || type == TYPE_4CHAR || type == TYPE_ARRAY_CHAR_OF_LENGTH_DEPENDING_ON_PREVIOUS_ARGS)
#define IS_ARRAY_SHORT(type)  (type == TYPE_ARRAY_SHORT || type == TYPE_1SHORT || type == TYPE_2SHORT || type == TYPE_3SHORT || type == TYPE_4SHORT || type == TYPE_ARRAY_SHORT_OF_LENGTH_DEPENDING_ON_PREVIOUS_ARGS)
#define IS_ARRAY_INT(type)  (type == TYPE_ARRAY_INT || type == TYPE_1INT || type == TYPE_2INT || type == TYPE_3INT || type == TYPE_4INT || type == TYPE_ARRAY_INT_OF_LENGTH_DEPENDING_ON_PREVIOUS_ARGS)
#define IS_ARRAY_FLOAT(type)  (type == TYPE_ARRAY_FLOAT || type == TYPE_1FLOAT || type == TYPE_2FLOAT || type == TYPE_3FLOAT || type == TYPE_4FLOAT || type == TYPE_16FLOAT || type == TYPE_ARRAY_FLOAT_OF_LENGTH_DEPENDING_ON_PREVIOUS_ARGS)
#define IS_ARRAY_DOUBLE(type)  (type == TYPE_ARRAY_DOUBLE || type == TYPE_1DOUBLE || type == TYPE_2DOUBLE || type == TYPE_3DOUBLE || type == TYPE_4DOUBLE || type == TYPE_16DOUBLE || type == TYPE_ARRAY_DOUBLE_OF_LENGTH_DEPENDING_ON_PREVIOUS_ARGS)

#define NB_MAX_TEXTURES 16
#define MY_GL_MAX_VERTEX_ATTRIBS_ARB 16
#define MY_GL_MAX_VERTEX_ATTRIBS_NV 16
#define MY_GL_MAX_VARIANT_POINTER_EXT 16

static int tab_args_type_length[] = {
    0,
    sizeof(char),
    sizeof(unsigned char),
    sizeof(short),
    sizeof(unsigned short),
    sizeof(int),
    sizeof(unsigned int),
    sizeof(float),
    sizeof(double),
    1 * sizeof(char),
    2 * sizeof(char),
    3 * sizeof(char),
    4 * sizeof(char),
    128 * sizeof(char),
    1 * sizeof(short),
    2 * sizeof(short),
    3 * sizeof(short),
    4 * sizeof(short),
    1 * sizeof(int),
    2 * sizeof(int),
    3 * sizeof(int),
    4 * sizeof(int),
    1 * sizeof(float),
    2 * sizeof(float),
    3 * sizeof(float),
    4 * sizeof(float),
    16 * sizeof(float),
    1 * sizeof(double),
    2 * sizeof(double),
    3 * sizeof(double),
    4 * sizeof(double),
    16 * sizeof(double),
    sizeof(int),
    sizeof(float),
    4 * sizeof(char),
    4 * sizeof(int),
    4 * sizeof(float),
    4 * sizeof(double),
    128 * sizeof(char),
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,

    /* the following sizes are the size of 1 element of the array */
    sizeof(char),
    sizeof(short),
    sizeof(int),
    sizeof(float),
    sizeof(double),
    sizeof(char),
    sizeof(short),
    sizeof(int),
    sizeof(float),
    sizeof(double),
};

typedef struct {
    int ret_type;
    int has_out_parameters;
    int nb_args;
    int args_type[0];
} Signature;

static const int _init32_signature[] =
    { TYPE_NONE, 1, 2, TYPE_INT, TYPE_OUT_1INT };
static const int _init64_signature[] =
    { TYPE_NONE, 1, 2, TYPE_INT, TYPE_OUT_1INT };

static const int _synchronize_signature[] = { TYPE_INT, 0, 0 };

static const int _serialized_calls_signature[] =
    { TYPE_NONE, 0, 1, TYPE_ARRAY_CHAR };

static const int _changeWindowState_signature[] =
    { TYPE_NONE, 0, 2, TYPE_INT, TYPE_INT };

static const int _moveResizeWindow_signature[] =
    { TYPE_NONE, 0, 2, TYPE_INT, TYPE_4INT };

static const int _send_cursor_signature[] =
    { TYPE_NONE, 0, 7, TYPE_INT, TYPE_INT,
    TYPE_INT, TYPE_INT,
    TYPE_INT, TYPE_INT,
    TYPE_ARRAY_INT
};

/* XVisualInfo* glXChooseVisual( Display *dpy, int screen, int *attribList ) */
static const int glXChooseVisual_signature[] =
    { TYPE_INT, 0, 3, TYPE_IN_IGNORED_POINTER, TYPE_INT, TYPE_ARRAY_INT };

/*GLXContext glXCreateContext( Display *dpy, XVisualInfo *vis,
                             GLXContext shareList, Bool direct )*/
static const int glXCreateContext_signature[] =
    { TYPE_INT, 0, 4, TYPE_IN_IGNORED_POINTER, TYPE_INT, TYPE_INT, TYPE_INT };

static const int glXCopyContext_signature[] =
    { TYPE_NONE, 0, 4, TYPE_IN_IGNORED_POINTER, TYPE_INT, TYPE_INT,
TYPE_INT };

/* void glXDestroyContext( Display *dpy, GLXContext ctx ) */
static const int glXDestroyContext_signature[] =
    { TYPE_NONE, 0, 2, TYPE_IN_IGNORED_POINTER, TYPE_INT };

/* Bool glXMakeCurrent( Display *dpy, GLXDrawable drawable, GLXContext ctx) */
//static const int glXMakeCurrent_signature[] = {TYPE_INT, 0, 3, TYPE_IN_IGNORED_POINTER, TYPE_INT, TYPE_INT};
/* making it asynchronous */
static const int glXMakeCurrent_signature[] =
    { TYPE_NONE, 0, 3, TYPE_IN_IGNORED_POINTER, TYPE_INT, TYPE_INT };

/*int glXGetConfig( Display *dpy, XVisualInfo *visual,
                  int attrib, int *value )*/
static const int glXGetConfig_signature[] =
    { TYPE_INT, 1, 4, TYPE_IN_IGNORED_POINTER, TYPE_INT, TYPE_INT,
TYPE_OUT_1INT };

/* "glXGetConfig_extended"(dpy, visual_id, int n, int* attribs, int* values, int* rets) */
static const int glXGetConfig_extended_signature[] =
    { TYPE_NONE, 1, 6, TYPE_IN_IGNORED_POINTER, TYPE_INT, TYPE_INT,
TYPE_ARRAY_INT, TYPE_OUT_ARRAY_INT, TYPE_OUT_ARRAY_INT };

/* void glXSwapBuffers( Display *dpy, GLXDrawable drawable ); */
static const int glXSwapBuffers_signature[] =
    { TYPE_NONE, 0, 2, TYPE_IN_IGNORED_POINTER, TYPE_INT };

/* Bool glXQueryVersion( Display *dpy, int *maj, int *min ) */
static const int glXQueryVersion_signature[] =
    { TYPE_INT, 1, 3, TYPE_IN_IGNORED_POINTER, TYPE_OUT_1INT, TYPE_OUT_1INT };

/* Bool glXQueryExtension( Display *dpy, int *errorBase, int *eventBase ) */
static const int glXQueryExtension_signature[] =
    { TYPE_INT, 1, 3, TYPE_IN_IGNORED_POINTER, TYPE_OUT_1INT, TYPE_OUT_1INT };

static const int glXWaitGL_signature[] = { TYPE_INT, 0, 0 };
static const int glXWaitX_signature[] = { TYPE_INT, 0, 0 };

/* GLX 1.1 and later */

/* const char *glXGetClientString( Display *dpy, int name ) */
static const int glXGetClientString_signature[] =
    { TYPE_CONST_CHAR, 0, 2, TYPE_IN_IGNORED_POINTER, TYPE_INT };

/*const char *glXQueryExtensionsString( Display *dpy, int screen ) */
static const int glXQueryExtensionsString_signature[] =
    { TYPE_CONST_CHAR, 0, 2, TYPE_IN_IGNORED_POINTER, TYPE_INT };

/* const char *glXQueryServerString( Display *dpy, int screen, int name ) */
static const int glXQueryServerString_signature[] =
    { TYPE_CONST_CHAR, 0, 3, TYPE_IN_IGNORED_POINTER, TYPE_INT, TYPE_INT };


static const int glXGetProcAddress_fake_signature[] =
    { TYPE_INT, 0, 1, TYPE_NULL_TERMINATED_STRING };

static const int glXGetProcAddress_global_fake_signature[] =
    { TYPE_NONE, 1, 3, TYPE_INT, TYPE_ARRAY_CHAR, TYPE_OUT_ARRAY_CHAR };


/* GLX 1.3 and later */

/*
GLXFBConfig *glXChooseFBConfig( Display *dpy, int screen,
                                       const int *attribList, int *nitems ); */
static const int glXChooseFBConfig_signature[] =
    { TYPE_INT, 1, 4, TYPE_IN_IGNORED_POINTER, TYPE_INT, TYPE_ARRAY_INT,
TYPE_OUT_1INT };

static const int glXChooseFBConfigSGIX_signature[] =
    { TYPE_INT, 1, 4, TYPE_IN_IGNORED_POINTER, TYPE_INT, TYPE_ARRAY_INT,
TYPE_OUT_1INT };

static const int glXGetFBConfigs_signature[] =
    { TYPE_INT, 1, 3, TYPE_IN_IGNORED_POINTER, TYPE_INT, TYPE_OUT_1INT };

/* "glXGetFBConfigAttrib_extended"(dpy, fbconfig, int n, int* attribs, int* values, int* rets) */
static const int glXGetFBConfigAttrib_extended_signature[] =
    { TYPE_NONE, 1, 6, TYPE_IN_IGNORED_POINTER, TYPE_INT, TYPE_INT,
TYPE_ARRAY_INT, TYPE_OUT_ARRAY_INT, TYPE_OUT_ARRAY_INT };


/* GLXPbuffer glXCreatePbuffer( Display *dpy, GLXFBConfig config,
                             const int *attribList ) */
static const int glXCreatePbuffer_signature[] =
    { TYPE_INT, 0, 3, TYPE_IN_IGNORED_POINTER, TYPE_INT, TYPE_ARRAY_INT };

static const int glXCreateGLXPbufferSGIX_signature[] =
    { TYPE_INT, 0, 5, TYPE_IN_IGNORED_POINTER, TYPE_INT, TYPE_INT, TYPE_INT,
TYPE_ARRAY_INT };

static const int glXDestroyPbuffer_signature[] =
    { TYPE_NONE, 0, 2, TYPE_IN_IGNORED_POINTER, TYPE_INT };

static const int glXDestroyGLXPbufferSGIX_signature[] =
    { TYPE_NONE, 0, 2, TYPE_IN_IGNORED_POINTER, TYPE_INT };

/* GLXContext glXCreateNewContext(Display * dpy
                               GLXFBConfig  config
                               int  renderType
                               GLXContext  ShareList
                               Bool  Direct) */
static const int glXCreateNewContext_signature[] =
    { TYPE_INT, 0, 5, TYPE_IN_IGNORED_POINTER, TYPE_INT, TYPE_INT, TYPE_INT,
TYPE_INT };

static const int glXCreateContextWithConfigSGIX_signature[] =
    { TYPE_INT, 0, 5, TYPE_IN_IGNORED_POINTER, TYPE_INT, TYPE_INT, TYPE_INT,
TYPE_INT };

/*XVisualInfo *glXGetVisualFromFBConfig( Display *dpy, GLXFBConfig config ) */
static const int glXGetVisualFromFBConfig_signature[] =
    { TYPE_INT, 0, 2, TYPE_IN_IGNORED_POINTER, TYPE_INT };

/*int glXGetFBConfigAttrib(Display *dpy, GLXFBConfig  config, int attribute, int *value)*/
static const int glXGetFBConfigAttrib_signature[] =
    { TYPE_INT, 1, 4, TYPE_IN_IGNORED_POINTER, TYPE_INT, TYPE_INT,
TYPE_OUT_1INT };

static const int glXGetFBConfigAttribSGIX_signature[] =
    { TYPE_INT, 1, 4, TYPE_IN_IGNORED_POINTER, TYPE_INT, TYPE_INT,
TYPE_OUT_1INT };

static const int glXQueryContext_signature[] =
    { TYPE_INT, 1, 4, TYPE_IN_IGNORED_POINTER, TYPE_INT, TYPE_INT,
TYPE_OUT_1INT };

static const int glXQueryGLXPbufferSGIX_signature[] =
    { TYPE_INT, 1, 4, TYPE_IN_IGNORED_POINTER, TYPE_INT, TYPE_INT,
TYPE_OUT_1INT };

static const int glXQueryDrawable_signature[] =
    { TYPE_NONE, 1, 4, TYPE_IN_IGNORED_POINTER, TYPE_INT, TYPE_INT,
TYPE_OUT_1INT };

/* void glXUseXFont( Font font, int first, int count, int list ) */
static const int glXUseXFont_signature[] =
    { TYPE_NONE, 0, 4, TYPE_INT, TYPE_INT, TYPE_INT, TYPE_INT };

/* Bool glXIsDirect( Display *dpy, GLXContext ctx ) */
static const int glXIsDirect_signature[] =
    { TYPE_CHAR, 0, 2, TYPE_IN_IGNORED_POINTER, TYPE_INT };

static const int glXGetScreenDriver_signature[] =
    { TYPE_CONST_CHAR, 0, 2, TYPE_IN_IGNORED_POINTER, TYPE_INT };

static const int glXGetDriverConfig_signature[] =
    { TYPE_CONST_CHAR, 0, 1, TYPE_NULL_TERMINATED_STRING };


static const int glXWaitVideoSyncSGI_signature[] =
    { TYPE_INT, 1, 3, TYPE_INT, TYPE_INT, TYPE_OUT_1INT };

static const int glXGetVideoSyncSGI_signature[] =
    { TYPE_INT, 1, 1, TYPE_OUT_1INT };

static const int glXSwapIntervalSGI_signature[] =
    { TYPE_INT, 0, 1, TYPE_INT };

static const int glXBindTexImageATI_signature[] =
    { TYPE_NONE, 0, 3, TYPE_IN_IGNORED_POINTER, TYPE_INT, TYPE_INT };
static const int glXReleaseTexImageATI_signature[] =
    { TYPE_NONE, 0, 3, TYPE_IN_IGNORED_POINTER, TYPE_INT, TYPE_INT };
static const int glXBindTexImageARB_signature[] =
    { TYPE_INT, 0, 3, TYPE_IN_IGNORED_POINTER, TYPE_INT, TYPE_INT };
static const int glXReleaseTexImageARB_signature[] =
    { TYPE_INT, 0, 3, TYPE_IN_IGNORED_POINTER, TYPE_INT, TYPE_INT };

/* const GLubyte * glGetString( GLenum name ) */
static const int glGetString_signature[] =
    { TYPE_CONST_CHAR, 0, 1, TYPE_INT };

/* void glShaderSourceARB (GLhandleARB handle , GLsizei size, const GLcharARB* *p_tab_prog, const GLint * tab_length) */
/* --> void glShaderSourceARB (GLhandleARB handle , GLsizei size, const GLcharARB* all_progs, const GLint * tab_length) */
static const int glShaderSourceARB_fake_signature[] =
    { TYPE_NONE, 0, 4, TYPE_INT, TYPE_INT, TYPE_ARRAY_CHAR, TYPE_ARRAY_INT };
static const int glShaderSource_fake_signature[] =
    { TYPE_NONE, 0, 4, TYPE_INT, TYPE_INT, TYPE_ARRAY_CHAR, TYPE_ARRAY_INT };

static const int glVertexPointer_fake_signature[] =
    { TYPE_NONE, 0, 6, TYPE_INT, TYPE_INT, TYPE_INT, TYPE_INT, TYPE_INT,
TYPE_ARRAY_CHAR };
static const int glNormalPointer_fake_signature[] =
    { TYPE_NONE, 0, 5, TYPE_INT, TYPE_INT, TYPE_INT, TYPE_INT,
TYPE_ARRAY_CHAR };
static const int glColorPointer_fake_signature[] =
    { TYPE_NONE, 0, 6, TYPE_INT, TYPE_INT, TYPE_INT, TYPE_INT, TYPE_INT,
TYPE_ARRAY_CHAR };
static const int glSecondaryColorPointer_fake_signature[] =
    { TYPE_NONE, 0, 6, TYPE_INT, TYPE_INT, TYPE_INT, TYPE_INT, TYPE_INT,
TYPE_ARRAY_CHAR };
static const int glIndexPointer_fake_signature[] =
    { TYPE_NONE, 0, 5, TYPE_INT, TYPE_INT, TYPE_INT, TYPE_INT,
TYPE_ARRAY_CHAR };
static const int glTexCoordPointer_fake_signature[] =
    { TYPE_NONE, 0, 7, TYPE_INT, TYPE_INT, TYPE_INT, TYPE_INT, TYPE_INT,
TYPE_INT, TYPE_ARRAY_CHAR };
static const int glEdgeFlagPointer_fake_signature[] =
    { TYPE_NONE, 0, 4, TYPE_INT, TYPE_INT, TYPE_INT, TYPE_ARRAY_CHAR };
static const int glVertexAttribPointerARB_fake_signature[] =
    { TYPE_NONE, 0, 8, TYPE_INT, TYPE_INT, TYPE_INT, TYPE_INT, TYPE_INT,
TYPE_INT, TYPE_INT, TYPE_ARRAY_CHAR };
static const int glVertexAttribPointerNV_fake_signature[] =
    { TYPE_NONE, 0, 7, TYPE_INT, TYPE_INT, TYPE_INT, TYPE_INT, TYPE_INT,
TYPE_INT, TYPE_ARRAY_CHAR };
static const int glWeightPointerARB_fake_signature[] =
    { TYPE_NONE, 0, 6, TYPE_INT, TYPE_INT, TYPE_INT, TYPE_INT, TYPE_INT,
TYPE_ARRAY_CHAR };
static const int glMatrixIndexPointerARB_fake_signature[] =
    { TYPE_NONE, 0, 6, TYPE_INT, TYPE_INT, TYPE_INT, TYPE_INT, TYPE_INT,
TYPE_ARRAY_CHAR };
static const int glFogCoordPointer_fake_signature[] =
    { TYPE_NONE, 0, 5, TYPE_INT, TYPE_INT, TYPE_INT, TYPE_INT,
TYPE_ARRAY_CHAR };
static const int glInterleavedArrays_fake_signature[] =
    { TYPE_NONE, 0, 5, TYPE_INT, TYPE_INT, TYPE_INT, TYPE_INT,
TYPE_ARRAY_CHAR };
static const int glElementPointerATI_fake_signature[] =
    { TYPE_NONE, 0, 3, TYPE_INT, TYPE_INT, TYPE_ARRAY_CHAR };
static const int glVariantPointerEXT_fake_signature[] =
    { TYPE_NONE, 0, 6, TYPE_INT, TYPE_INT, TYPE_INT, TYPE_INT, TYPE_INT,
TYPE_ARRAY_CHAR };
static const int glTuxRacerDrawElements_fake_signature[] =
    { TYPE_NONE, 0, 4, TYPE_INT, TYPE_INT, TYPE_INT, TYPE_ARRAY_CHAR };
static const int glVertexAndNormalPointer_fake_signature[] =
    { TYPE_NONE, 0, 7, TYPE_INT, TYPE_INT, TYPE_INT, TYPE_INT, TYPE_INT,
TYPE_INT, TYPE_ARRAY_CHAR };
static const int glTexCoordPointer01_fake_signature[] =
    { TYPE_NONE, 0, 5, TYPE_INT, TYPE_INT, TYPE_INT, TYPE_INT,
TYPE_ARRAY_CHAR };
static const int glTexCoordPointer012_fake_signature[] =
    { TYPE_NONE, 0, 5, TYPE_INT, TYPE_INT, TYPE_INT, TYPE_INT,
TYPE_ARRAY_CHAR };
static const int glVertexNormalPointerInterlaced_fake_signature[] =
    { TYPE_NONE, 0, 8, TYPE_INT, TYPE_INT, TYPE_INT, TYPE_INT, TYPE_INT,
TYPE_INT, TYPE_INT, TYPE_ARRAY_CHAR };
static const int glVertexNormalColorPointerInterlaced_fake_signature[] =
    { TYPE_NONE, 0, 11, TYPE_INT, TYPE_INT, TYPE_INT, TYPE_INT, TYPE_INT,
TYPE_INT, TYPE_INT, TYPE_INT, TYPE_INT, TYPE_INT, TYPE_ARRAY_CHAR };
static const int glVertexColorTexCoord0PointerInterlaced_fake_signature[] =
    { TYPE_NONE, 0, 12, TYPE_INT, TYPE_INT, TYPE_INT, TYPE_INT, TYPE_INT,
TYPE_INT, TYPE_INT, TYPE_INT, TYPE_INT, TYPE_INT, TYPE_INT, TYPE_ARRAY_CHAR };
static const int glVertexNormalTexCoord0PointerInterlaced_fake_signature[] =
    { TYPE_NONE, 0, 11, TYPE_INT, TYPE_INT, TYPE_INT, TYPE_INT, TYPE_INT,
TYPE_INT, TYPE_INT, TYPE_INT, TYPE_INT, TYPE_INT, TYPE_ARRAY_CHAR };
static const int glVertexNormalTexCoord01PointerInterlaced_fake_signature[] =
    { TYPE_NONE, 0, 14, TYPE_INT, TYPE_INT, TYPE_INT, TYPE_INT, TYPE_INT,
TYPE_INT, TYPE_INT, TYPE_INT, TYPE_INT, TYPE_INT, TYPE_INT, TYPE_INT, TYPE_INT, TYPE_ARRAY_CHAR };
static const int glVertexNormalTexCoord012PointerInterlaced_fake_signature[] =
    { TYPE_NONE, 0, 17, TYPE_INT, TYPE_INT, TYPE_INT, TYPE_INT, TYPE_INT,
TYPE_INT, TYPE_INT, TYPE_INT, TYPE_INT, TYPE_INT, TYPE_INT, TYPE_INT, TYPE_INT, TYPE_INT, TYPE_INT, TYPE_INT, TYPE_ARRAY_CHAR };
static const int
    glVertexNormalColorTexCoord0PointerInterlaced_fake_signature[] =
    { TYPE_NONE, 0, 14, TYPE_INT, TYPE_INT, TYPE_INT, TYPE_INT, TYPE_INT,
TYPE_INT, TYPE_INT, TYPE_INT, TYPE_INT, TYPE_INT, TYPE_INT, TYPE_INT, TYPE_INT,
TYPE_ARRAY_CHAR };
static const int
    glVertexNormalColorTexCoord01PointerInterlaced_fake_signature[] =
    { TYPE_NONE, 0, 17, TYPE_INT, TYPE_INT, TYPE_INT, TYPE_INT, TYPE_INT,
TYPE_INT, TYPE_INT, TYPE_INT, TYPE_INT, TYPE_INT, TYPE_INT, TYPE_INT, TYPE_INT,
TYPE_INT, TYPE_INT, TYPE_INT, TYPE_ARRAY_CHAR };
static const int
    glVertexNormalColorTexCoord012PointerInterlaced_fake_signature[] =
    { TYPE_NONE, 0, 20, TYPE_INT, TYPE_INT, TYPE_INT, TYPE_INT, TYPE_INT,
TYPE_INT, TYPE_INT, TYPE_INT, TYPE_INT, TYPE_INT, TYPE_INT, TYPE_INT, TYPE_INT,
TYPE_INT, TYPE_INT, TYPE_INT, TYPE_INT, TYPE_INT, TYPE_INT, TYPE_ARRAY_CHAR };

static const int glGenTextures_fake_signature[] =
    { TYPE_NONE, 0, 1, TYPE_INT };
static const int glGenBuffersARB_fake_signature[] =
    { TYPE_NONE, 0, 1, TYPE_INT };
static const int glGenLists_fake_signature[] = { TYPE_NONE, 0, 1, TYPE_INT };

static const int _glDrawElements_buffer_signature[] =
    { TYPE_NONE, 0, 4, TYPE_INT, TYPE_INT, TYPE_INT, TYPE_INT };
static const int _glDrawRangeElements_buffer_signature[] =
    { TYPE_NONE, 0, 6, TYPE_INT, TYPE_INT, TYPE_INT, TYPE_INT, TYPE_INT,
TYPE_INT };
static const int _glMultiDrawElements_buffer_signature[] =
    { TYPE_NONE, 0, 5, TYPE_INT, TYPE_ARRAY_INT, TYPE_INT, TYPE_ARRAY_INT,
TYPE_INT };

static const int _glVertexPointer_buffer_signature[] =
    { TYPE_NONE, 0, 4, TYPE_INT, TYPE_INT, TYPE_INT, TYPE_INT };
static const int _glNormalPointer_buffer_signature[] =
    { TYPE_NONE, 0, 3, TYPE_INT, TYPE_INT, TYPE_INT };
static const int _glColorPointer_buffer_signature[] =
    { TYPE_NONE, 0, 4, TYPE_INT, TYPE_INT, TYPE_INT, TYPE_INT };
static const int _glSecondaryColorPointer_buffer_signature[] =
    { TYPE_NONE, 0, 4, TYPE_INT, TYPE_INT, TYPE_INT, TYPE_INT };
static const int _glIndexPointer_buffer_signature[] =
    { TYPE_NONE, 0, 3, TYPE_INT, TYPE_INT, TYPE_INT };
static const int _glTexCoordPointer_buffer_signature[] =
    { TYPE_NONE, 0, 4, TYPE_INT, TYPE_INT, TYPE_INT, TYPE_INT };
static const int _glEdgeFlagPointer_buffer_signature[] =
    { TYPE_NONE, 0, 2, TYPE_INT, TYPE_INT };
static const int _glVertexAttribPointerARB_buffer_signature[] =
    { TYPE_NONE, 0, 6, TYPE_INT, TYPE_INT, TYPE_INT, TYPE_INT, TYPE_INT,
TYPE_INT };
static const int _glWeightPointerARB_buffer_signature[] =
    { TYPE_NONE, 0, 4, TYPE_INT, TYPE_INT, TYPE_INT, TYPE_INT };
static const int _glMatrixIndexPointerARB_buffer_signature[] =
    { TYPE_NONE, 0, 4, TYPE_INT, TYPE_INT, TYPE_INT, TYPE_INT };
static const int _glFogCoordPointer_buffer_signature[] =
    { TYPE_NONE, 0, 3, TYPE_INT, TYPE_INT, TYPE_INT };
static const int _glVariantPointerEXT_buffer_signature[] =
    { TYPE_NONE, 0, 4, TYPE_INT, TYPE_INT, TYPE_INT, TYPE_INT };

static const int _glReadPixels_pbo_signature[] =
    { TYPE_INT, 0, 7, TYPE_INT, TYPE_INT, TYPE_INT, TYPE_INT, TYPE_INT,
TYPE_INT, TYPE_INT };
static const int _glDrawPixels_pbo_signature[] =
    { TYPE_NONE, 0, 5, TYPE_INT, TYPE_INT, TYPE_INT, TYPE_INT, TYPE_INT };
static const int _glMapBufferARB_fake_signature[] =
    { TYPE_INT, 1, 3, TYPE_INT, TYPE_INT, TYPE_OUT_ARRAY_CHAR };

static const int _glSelectBuffer_fake_signature[] =
    { TYPE_NONE, 0, 1, TYPE_INT };
static const int _glGetSelectBuffer_fake_signature[] =
    { TYPE_NONE, 1, 1, TYPE_ARRAY_CHAR };
static const int _glFeedbackBuffer_fake_signature[] =
    { TYPE_NONE, 0, 2, TYPE_INT, TYPE_INT };
static const int _glGetFeedbackBuffer_fake_signature[] =
    { TYPE_NONE, 1, 1, TYPE_ARRAY_CHAR };

static const int _glGetError_fake_signature[] = { TYPE_NONE, 0, 0 };

#define timesynchro_func    -1
#define memorize_array_func -2
#define reuse_array_func    -3

#include "gl_func.h"

static GLint __glTexParameter_size(FILE *err_file, GLenum pname)
{
    switch (pname) {
    case GL_TEXTURE_MAG_FILTER:
    case GL_TEXTURE_MIN_FILTER:
    case GL_TEXTURE_WRAP_S:
    case GL_TEXTURE_WRAP_T:
    case GL_TEXTURE_PRIORITY:
    case GL_TEXTURE_WRAP_R:
    case GL_TEXTURE_COMPARE_FAIL_VALUE_ARB:
/*      case GL_SHADOW_AMBIENT_SGIX:*/
    case GL_TEXTURE_MIN_LOD:
    case GL_TEXTURE_MAX_LOD:
    case GL_TEXTURE_BASE_LEVEL:
    case GL_TEXTURE_MAX_LEVEL:
    case GL_TEXTURE_CLIPMAP_FRAME_SGIX:
    case GL_TEXTURE_LOD_BIAS_S_SGIX:
    case GL_TEXTURE_LOD_BIAS_T_SGIX:
    case GL_TEXTURE_LOD_BIAS_R_SGIX:
    case GL_GENERATE_MIPMAP:
/*      case GL_GENERATE_MIPMAP_SGIS:*/
    case GL_TEXTURE_COMPARE_SGIX:
    case GL_TEXTURE_COMPARE_OPERATOR_SGIX:
    case GL_TEXTURE_MAX_CLAMP_S_SGIX:
    case GL_TEXTURE_MAX_CLAMP_T_SGIX:
    case GL_TEXTURE_MAX_CLAMP_R_SGIX:
    case GL_TEXTURE_MAX_ANISOTROPY_EXT:
    case GL_TEXTURE_LOD_BIAS:
/*      case GL_TEXTURE_LOD_BIAS_EXT:*/
    case GL_DEPTH_TEXTURE_MODE:
/*      case GL_DEPTH_TEXTURE_MODE_ARB:*/
    case GL_TEXTURE_COMPARE_MODE:
/*      case GL_TEXTURE_COMPARE_MODE_ARB:*/
    case GL_TEXTURE_COMPARE_FUNC:
/*      case GL_TEXTURE_COMPARE_FUNC_ARB:*/
    case GL_TEXTURE_UNSIGNED_REMAP_MODE_NV:
        return 1;
    case GL_TEXTURE_CLIPMAP_CENTER_SGIX:
    case GL_TEXTURE_CLIPMAP_OFFSET_SGIX:
        return 2;
    case GL_TEXTURE_CLIPMAP_VIRTUAL_DEPTH_SGIX:
        return 3;
    case GL_TEXTURE_BORDER_COLOR:
    case GL_POST_TEXTURE_FILTER_BIAS_SGIX:
    case GL_POST_TEXTURE_FILTER_SCALE_SGIX:
        return 4;
    default:
        fprintf(err_file, "unhandled pname = %d\n", pname);
        return 0;
    }
}

static int __glLight_size(FILE *err_file, GLenum pname)
{
    switch (pname) {
    case GL_AMBIENT:
    case GL_DIFFUSE:
    case GL_SPECULAR:
    case GL_POSITION:
        return 4;
        break;

    case GL_SPOT_DIRECTION:
        return 3;
        break;

    case GL_SPOT_EXPONENT:
    case GL_SPOT_CUTOFF:
    case GL_CONSTANT_ATTENUATION:
    case GL_LINEAR_ATTENUATION:
    case GL_QUADRATIC_ATTENUATION:
        return 1;
        break;

    default:
        fprintf(err_file, "unhandled pname = %d\n", pname);
        return 0;
    }
}

static int __glMaterial_size(FILE *err_file, GLenum pname)
{
    switch (pname) {
    case GL_AMBIENT:
    case GL_DIFFUSE:
    case GL_SPECULAR:
    case GL_EMISSION:
    case GL_AMBIENT_AND_DIFFUSE:
        return 4;
        break;

    case GL_SHININESS:
        return 1;
        break;

    case GL_COLOR_INDEXES:
        return 3;
        break;

    default:
        fprintf(err_file, "unhandled pname = %d\n", pname);
        return 0;
    }
}

static inline int compute_arg_length(FILE *err_file, int func_number,
                                     int arg_i, arg_t *args)
{
    Signature *signature = (Signature *) tab_opengl_calls[func_number];
    int *args_type = signature->args_type;

    switch (func_number) {
    case glProgramNamedParameter4fNV_func:
    case glProgramNamedParameter4dNV_func:
    case glProgramNamedParameter4fvNV_func:
    case glProgramNamedParameter4dvNV_func:
    case glGetProgramNamedParameterfvNV_func:
    case glGetProgramNamedParameterdvNV_func:
        if (arg_i == 2)
            return 1 * args[arg_i - 1] *
                    tab_args_type_length[args_type[arg_i]];
        break;

    case glProgramStringARB_func:
    case glLoadProgramNV_func:
    case glGenProgramsNV_func:
    case glDeleteProgramsNV_func:
    case glGenProgramsARB_func:
    case glDeleteProgramsARB_func:
    case glRequestResidentProgramsNV_func:
    case glDrawBuffers_func:
    case glDrawBuffersARB_func:
    case glDrawBuffersATI_func:
    case glDeleteBuffers_func:
    case glDeleteBuffersARB_func:
    case glDeleteTextures_func:
    case glDeleteTexturesEXT_func:
    case glGenFramebuffersEXT_func:
    case glDeleteFramebuffersEXT_func:
    case glGenRenderbuffersEXT_func:
    case glDeleteRenderbuffersEXT_func:
    case glGenQueries_func:
    case glGenQueriesARB_func:
    case glDeleteQueries_func:
    case glDeleteQueriesARB_func:
    case glGenOcclusionQueriesNV_func:
    case glDeleteOcclusionQueriesNV_func:
    case glGenFencesNV_func:
    case glDeleteFencesNV_func:
    case glUniform1fv_func:
    case glUniform1iv_func:
    case glUniform1fvARB_func:
    case glUniform1ivARB_func:
    case glUniform1uivEXT_func:
    case glVertexAttribs1dvNV_func:
    case glVertexAttribs1fvNV_func:
    case glVertexAttribs1svNV_func:
    case glVertexAttribs1hvNV_func:
    case glWeightbvARB_func:
    case glWeightsvARB_func:
    case glWeightivARB_func:
    case glWeightfvARB_func:
    case glWeightdvARB_func:
    case glWeightubvARB_func:
    case glWeightusvARB_func:
    case glWeightuivARB_func:
    case glPixelMapfv_func:
    case glPixelMapuiv_func:
    case glPixelMapusv_func:
    case glProgramBufferParametersfvNV_func:
    case glProgramBufferParametersIivNV_func:
    case glProgramBufferParametersIuivNV_func:
    case glTransformFeedbackAttribsNV_func:
    case glTransformFeedbackVaryingsNV_func:
        if (arg_i == signature->nb_args - 1)
            return 1 * args[arg_i - 1] *
                    tab_args_type_length[args_type[arg_i]];
        break;

    case glUniform2fv_func:
    case glUniform2iv_func:
    case glUniform2fvARB_func:
    case glUniform2ivARB_func:
    case glUniform2uivEXT_func:
    case glVertexAttribs2dvNV_func:
    case glVertexAttribs2fvNV_func:
    case glVertexAttribs2svNV_func:
    case glVertexAttribs2hvNV_func:
    case glDetailTexFuncSGIS_func:
    case glSharpenTexFuncSGIS_func:
        if (arg_i == signature->nb_args - 1)
            return 2 * args[arg_i - 1] *
                    tab_args_type_length[args_type[arg_i]];
        break;

    case glUniform3fv_func:
    case glUniform3iv_func:
    case glUniform3fvARB_func:
    case glUniform3ivARB_func:
    case glUniform3uivEXT_func:
    case glVertexAttribs3dvNV_func:
    case glVertexAttribs3fvNV_func:
    case glVertexAttribs3svNV_func:
    case glVertexAttribs3hvNV_func:
        if (arg_i == signature->nb_args - 1)
            return 3 * args[arg_i - 1] *
                    tab_args_type_length[args_type[arg_i]];
        break;

    case glUniform4fv_func:
    case glUniform4iv_func:
    case glUniform4fvARB_func:
    case glUniform4ivARB_func:
    case glUniform4uivEXT_func:
    case glVertexAttribs4dvNV_func:
    case glVertexAttribs4fvNV_func:
    case glVertexAttribs4svNV_func:
    case glVertexAttribs4hvNV_func:
    case glVertexAttribs4ubvNV_func:
    case glProgramParameters4fvNV_func:
    case glProgramParameters4dvNV_func:
    case glProgramLocalParameters4fvEXT_func:
    case glProgramEnvParameters4fvEXT_func:
    case glProgramLocalParametersI4ivNV_func:
    case glProgramLocalParametersI4uivNV_func:
    case glProgramEnvParametersI4ivNV_func:
    case glProgramEnvParametersI4uivNV_func:
        if (arg_i == signature->nb_args - 1)
            return 4 * args[arg_i - 1] *
                    tab_args_type_length[args_type[arg_i]];
        break;

    case glPrioritizeTextures_func:
    case glPrioritizeTexturesEXT_func:
    case glAreProgramsResidentNV_func:
    case glAreTexturesResident_func:
    case glAreTexturesResidentEXT_func:
        if (arg_i == 1 || arg_i == 2)
            return args[0] * tab_args_type_length[args_type[arg_i]];
        break;

    case glLightfv_func:
    case glLightiv_func:
    case glGetLightfv_func:
    case glGetLightiv_func:
    case glFragmentLightfvSGIX_func:
    case glFragmentLightivSGIX_func:
    case glGetFragmentLightfvSGIX_func:
    case glGetFragmentLightivSGIX_func:
        if (arg_i == signature->nb_args - 1)
            return __glLight_size(err_file,
                                  args[arg_i -
                                       1]) *
                tab_args_type_length[args_type[arg_i]];
        break;

    case glLightModelfv_func:
    case glLightModeliv_func:
        if (arg_i == signature->nb_args - 1)
            return ((args[arg_i - 1] ==
                     GL_LIGHT_MODEL_AMBIENT) ? 4 : 1) *
                tab_args_type_length[args_type[arg_i]];
        break;

    case glFragmentLightModelfvSGIX_func:
    case glFragmentLightModelivSGIX_func:
        if (arg_i == signature->nb_args - 1)
            return ((args[arg_i - 1] ==
                     GL_FRAGMENT_LIGHT_MODEL_AMBIENT_SGIX) ? 4 : 1) *
                tab_args_type_length[args_type[arg_i]];
        break;

    case glMaterialfv_func:
    case glMaterialiv_func:
    case glGetMaterialfv_func:
    case glGetMaterialiv_func:
    case glFragmentMaterialfvSGIX_func:
    case glFragmentMaterialivSGIX_func:
    case glGetFragmentMaterialfvSGIX_func:
    case glGetFragmentMaterialivSGIX_func:
        if (arg_i == signature->nb_args - 1)
            return __glMaterial_size(err_file,
                                     args[arg_i -
                                          1]) *
                tab_args_type_length[args_type[arg_i]];
        break;

    case glTexParameterfv_func:
    case glTexParameteriv_func:
    case glGetTexParameterfv_func:
    case glGetTexParameteriv_func:
    case glTexParameterIivEXT_func:
    case glTexParameterIuivEXT_func:
    case glGetTexParameterIivEXT_func:
    case glGetTexParameterIuivEXT_func:
        if (arg_i == signature->nb_args - 1)
            return __glTexParameter_size(err_file,
                                         args[arg_i -
                                              1]) *
                tab_args_type_length[args_type[arg_i]];
        break;

    case glFogiv_func:
    case glFogfv_func:
        if (arg_i == signature->nb_args - 1)
            return ((args[arg_i - 1] ==
                     GL_FOG_COLOR) ? 4 : 1) *
                tab_args_type_length[args_type[arg_i]];
        break;

    case glTexGendv_func:
    case glTexGenfv_func:
    case glTexGeniv_func:
    case glGetTexGendv_func:
    case glGetTexGenfv_func:
    case glGetTexGeniv_func:
        if (arg_i == signature->nb_args - 1)
            return ((args[arg_i - 1] ==
                     GL_TEXTURE_GEN_MODE) ? 1 : 4) *
                tab_args_type_length[args_type[arg_i]];
        break;

    case glTexEnvfv_func:
    case glTexEnviv_func:
    case glGetTexEnvfv_func:
    case glGetTexEnviv_func:
        if (arg_i == signature->nb_args - 1)
            return ((args[arg_i - 1] ==
                     GL_TEXTURE_ENV_MODE) ? 1 : 4) *
                tab_args_type_length[args_type[arg_i]];
        break;

    case glConvolutionParameterfv_func:
    case glConvolutionParameteriv_func:
    case glGetConvolutionParameterfv_func:
    case glGetConvolutionParameteriv_func:
    case glConvolutionParameterfvEXT_func:
    case glConvolutionParameterivEXT_func:
    case glGetConvolutionParameterfvEXT_func:
    case glGetConvolutionParameterivEXT_func:
        if (arg_i == signature->nb_args - 1)
            return ((args[arg_i - 1] == GL_CONVOLUTION_BORDER_COLOR ||
                     args[arg_i - 1] == GL_CONVOLUTION_FILTER_SCALE ||
                     args[arg_i - 1] ==
                     GL_CONVOLUTION_FILTER_BIAS) ? 4 : 1) *
                tab_args_type_length[args_type[arg_i]];
        break;

    case glGetVertexAttribfvARB_func:
    case glGetVertexAttribfvNV_func:
    case glGetVertexAttribfv_func:
    case glGetVertexAttribdvARB_func:
    case glGetVertexAttribdvNV_func:
    case glGetVertexAttribdv_func:
    case glGetVertexAttribivARB_func:
    case glGetVertexAttribivNV_func:
    case glGetVertexAttribiv_func:
    case glGetVertexAttribIivEXT_func:
    case glGetVertexAttribIuivEXT_func:
        if (arg_i == signature->nb_args - 1)
            return ((args[arg_i - 1] ==
                     GL_CURRENT_VERTEX_ATTRIB_ARB) ? 4 : 1) *
                tab_args_type_length[args_type[arg_i]];
        break;


    case glPointParameterfv_func:
    case glPointParameterfvEXT_func:
    case glPointParameterfvARB_func:
    case glPointParameterfvSGIS_func:
    case glPointParameteriv_func:
    case glPointParameterivEXT_func:
        if (arg_i == signature->nb_args - 1)
            return ((args[arg_i - 1] ==
                     GL_POINT_DISTANCE_ATTENUATION) ? 3 : 1) *
                tab_args_type_length[args_type[arg_i]];
        break;

    case glUniformMatrix2fv_func:
    case glUniformMatrix2fvARB_func:
        if (arg_i == signature->nb_args - 1)
            return 2 * 2 * args[1] * tab_args_type_length[args_type[arg_i]];
        break;

    case glUniformMatrix3fv_func:
    case glUniformMatrix3fvARB_func:
        if (arg_i == signature->nb_args - 1)
            return 3 * 3 * args[1] * tab_args_type_length[args_type[arg_i]];
        break;

    case glUniformMatrix4fv_func:
    case glUniformMatrix4fvARB_func:
        if (arg_i == signature->nb_args - 1)
            return 4 * 4 * args[1] * tab_args_type_length[args_type[arg_i]];
        break;

    case glUniformMatrix2x3fv_func:
    case glUniformMatrix3x2fv_func:
        if (arg_i == signature->nb_args - 1)
            return 2 * 3 * args[1] * tab_args_type_length[args_type[arg_i]];
        break;

    case glUniformMatrix2x4fv_func:
    case glUniformMatrix4x2fv_func:
        if (arg_i == signature->nb_args - 1)
            return 2 * 4 * args[1] * tab_args_type_length[args_type[arg_i]];
        break;

    case glUniformMatrix3x4fv_func:
    case glUniformMatrix4x3fv_func:
        if (arg_i == signature->nb_args - 1)
            return 3 * 4 * args[1] * tab_args_type_length[args_type[arg_i]];
        break;

    case glSpriteParameterivSGIX_func:
    case glSpriteParameterfvSGIX_func:
        if (arg_i == signature->nb_args - 1)
            return ((args[arg_i - 1] ==
                     GL_SPRITE_MODE_SGIX) ? 1 : 3) *
                tab_args_type_length[args_type[arg_i]];
        break;

    default:
        break;
    }

    fprintf(err_file,
            "invalid combination for compute_arg_length : func_number=%d, arg_i=%d\n",
            func_number, arg_i);
    return 0;
}

#define IS_NULL_POINTER_OK_FOR_FUNC(func_number) \
                 (func_number == glBitmap_func || \
                  func_number == _send_cursor_func || \
                  func_number == glTexImage1D_func || \
                  func_number == glTexImage2D_func || \
                  func_number == glTexImage3D_func || \
                  func_number == glBufferDataARB_func || \
                  func_number == glNewObjectBufferATI_func)

#endif // INCLUDE_OPENGL_FUNC_H

