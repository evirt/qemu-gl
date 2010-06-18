/*
 *  Host-side implementation of GL/GLX API
 *
 *  Copyright (c) 2006,2007 Even Rouault
 *  Copyright (c) 2009,2010 Ian Molton <ian.molton@collabora.co.uk>
 *  Copyright (c) 2009,2010 Gordon Williams <gordon.williams@collabora.co.uk>
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
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISiNG FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <sys/types.h> // for pid_t
/* GW: dyngen-exec.h defines its own version of stuff that is in stdio.h - 
   only it misses things and is mildly different to stdio \o/. Hence
   don't include stdio and make our own defines. */
//#include <stdio.h>
#ifdef _WIN32
#define DEBUGF(...) printf(__VA_ARGS__)
#else
extern struct FILE *stderr;		/* Standard error output stream.  */
#define DEBUGF(...) fprintf(stderr, __VA_ARGS__)
#endif

#define GL_GLEXT_PROTOTYPES
#define GLX_GLXEXT_PROTOTYPES
#include <mesa_gl.h>

#include "opengl_func.h"
#include "mesa_mipmap.h"
#include "opengl_process.h"
#include "opengl_utils.h"
#include "gloffscreen.h"


// FIXME
typedef int Bool;
typedef int XID;
typedef struct Display Display;
const Bool True = 1;
const Bool False = 0;
const int None = 0;
typedef struct __GLXFBConfigRec GLXFBConfig;
struct __GLXFBConfigRec {
  int formatFlags;
};

#define GLX_VENDOR              1
#define GLX_VERSION             2
#define GLX_EXTENSIONS          3

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
/*
 * GLX 1.4 and later:
 */
#define GLX_SAMPLE_BUFFERS              0x186a0 /*100000*/
#define GLX_SAMPLES                     0x186a1 /*100001*/
// FIXME

/* We'll say the XVisual Id is actually just an index into here */
const GLXFBConfig FBCONFIGS[] = {
    {GLO_FF_ALPHA|GLO_FF_BITS_32},
    {GLO_FF_ALPHA|GLO_FF_BITS_32|GLO_FF_DEPTH_24},
    {GLO_FF_ALPHA|GLO_FF_BITS_32|GLO_FF_DEPTH_24|GLO_FF_STENCIL_8},

    {GLO_FF_BITS_32},
    {GLO_FF_BITS_32|GLO_FF_DEPTH_24},
    {GLO_FF_BITS_32|GLO_FF_DEPTH_24|GLO_FF_STENCIL_8},

    {GLO_FF_BITS_24},
    {GLO_FF_BITS_24|GLO_FF_DEPTH_24},
    {GLO_FF_BITS_24|GLO_FF_DEPTH_24|GLO_FF_STENCIL_8},
/*
    {GLO_FF_BITS_16},
    {GLO_FF_BITS_16|GLO_FF_DEPTH_24},
    {GLO_FF_BITS_16|GLO_FF_DEPTH_24|GLO_FF_STENCIL_8},*/
};


#define FGLX_VERSION_STRING "1.2"
#define FGLX_VERSION_MAJOR 1
#define FGLX_VERSION_MINOR 2


void *qemu_malloc(size_t size);
void *qemu_realloc(void *ptr, size_t size);
void qemu_free(void *ptr);

#define glGetError() 0

#define GET_EXT_PTR(type, funcname, args_decl) \
      static int detect_##funcname = 0; \
      static type(*ptr_func_##funcname)args_decl = NULL; \
      if (detect_##funcname == 0) \
      { \
        detect_##funcname = 1; \
        ptr_func_##funcname = (type(*)args_decl)glo_getprocaddress((const GLubyte*)#funcname); \
        assert (ptr_func_##funcname); \
      }

#define GET_EXT_PTR_NO_FAIL(type, funcname, args_decl) \
      static int detect_##funcname = 0; \
      static type(*ptr_func_##funcname)args_decl = NULL; \
      if (detect_##funcname == 0) \
      { \
        detect_##funcname = 1; \
        ptr_func_##funcname = (type(*)args_decl)glo_getprocaddress((const GLubyte*)#funcname); \
      }

static void *get_glu_ptr(const char *name)
{
    static void *handle = (void *) -1;

    if (handle == (void *) -1) {
#ifndef WIN32
        handle = dlopen("libGLU.so", RTLD_LAZY);
        if (!handle)
            DEBUGF("can't load libGLU.so : %s\n", dlerror());
#else
        handle = (void *) LoadLibrary("glu32.dll");
        if (!handle)
            DEBUGF("can't load glu32.dll\n");
#endif
    }
    if (handle) {
#ifndef WIN32
        return dlsym(handle, name);
#else
        return GetProcAddress(handle, name);
#endif
    }
    return NULL;
}

#define GET_GLU_PTR(type, funcname, args_decl) \
      static int detect_##funcname = 0; \
      static type(*ptr_func_##funcname)args_decl = NULL; \
      if (detect_##funcname == 0) \
      { \
        detect_##funcname = 1; \
        ptr_func_##funcname = (type(*)args_decl)get_glu_ptr(#funcname); \
      }

int display_function_call = 0;
extern int kill_process;

typedef struct {
    void *key;
    void *value;
} Assoc;

#define MAX_HANDLED_PROCESS 100
#define MAX_ASSOC_SIZE 100

#define MAX_FBCONFIG 10

#define MAX(a, b) (((a) > (b)) ? (a) : (b))
#define DIM(X) (sizeof(X) / sizeof(X[0]))

typedef struct {
    GLbitfield mask;
    int activeTextureIndex;
} ClientState;

#define MAX_CLIENT_STATE_STACK_SIZE 16

typedef void *ClientGLXDrawable;

typedef struct GLState GLState;

typedef struct QGloSurface {
  GLState *glstate;
  GloSurface *surface;
  ClientGLXDrawable *client_drawable;
  int ready;
  int ref;
  QTAILQ_ENTRY(QGloSurface) next;
} QGloSurface;

struct GLState {
    int ref;
    int fake_ctxt;
    int fake_shareList;

    GloContext *context; // context (owned by this)
    QGloSurface *current_qsurface; // current rendering surface/drawable
    QTAILQ_HEAD(, QGloSurface) qsurfaces; // list of surfaces/drawables for
                                          // this context

    void *vertexPointer;
    void *normalPointer;
    void *colorPointer;
    void *secondaryColorPointer;
    void *indexPointer;
    void *texCoordPointer[NB_MAX_TEXTURES];
    void *edgeFlagPointer;
    void *vertexAttribPointer[MY_GL_MAX_VERTEX_ATTRIBS_ARB];
    void *vertexAttribPointerNV[MY_GL_MAX_VERTEX_ATTRIBS_NV];
    void *weightPointer;
    void *matrixIndexPointer;
    void *fogCoordPointer;
    void *variantPointerEXT[MY_GL_MAX_VARIANT_POINTER_EXT];
    void *interleavedArrays;
    void *elementPointerATI;

    int vertexPointerSize;
    int normalPointerSize;
    int colorPointerSize;
    int secondaryColorPointerSize;
    int indexPointerSize;
    int texCoordPointerSize[NB_MAX_TEXTURES];
    int edgeFlagPointerSize;
    int vertexAttribPointerSize[MY_GL_MAX_VERTEX_ATTRIBS_ARB];
    int vertexAttribPointerNVSize[MY_GL_MAX_VERTEX_ATTRIBS_NV];
    int weightPointerSize;
    int matrixIndexPointerSize;
    int fogCoordPointerSize;
    int variantPointerEXTSize[MY_GL_MAX_VARIANT_POINTER_EXT];
    int interleavedArraysSize;
    int elementPointerATISize;

    int selectBufferSize;
    void *selectBufferPtr;
    int feedbackBufferSize;
    void *feedbackBufferPtr;

    ClientState clientStateStack[MAX_CLIENT_STATE_STACK_SIZE];
    int clientStateSp;
    int activeTextureIndex;

    unsigned int ownTabTextures[32768];
    unsigned int *tabTextures;
    RangeAllocator ownTextureAllocator;
    RangeAllocator *textureAllocator;

    unsigned int ownTabBuffers[32768];
    unsigned int *tabBuffers;
    RangeAllocator ownBufferAllocator;
    RangeAllocator *bufferAllocator;

    unsigned int ownTabLists[32768];
    unsigned int *tabLists;
    RangeAllocator ownListAllocator;
    RangeAllocator *listAllocator;

};

typedef struct {
    ProcessStruct p;

    int next_available_context_number;
#ifdef ALLOW_PBUFFER
    int next_available_pbuffer_number;
#endif

    int nb_states;
    GLState default_state;
    GLState **glstates;
    GLState *current_state;

    int nfbconfig;
    const GLXFBConfig *fbconfigs[MAX_FBCONFIG];
    int fbconfigs_max[MAX_FBCONFIG];
    int nfbconfig_total;

#ifdef ALLOW_PBUFFER
    Assoc association_fakepbuffer_pbuffer[MAX_ASSOC_SIZE];
#endif

    int primitive;
    int bufsize;
    int bufstart;
    arg_t *cmdbuf;
} ProcessState;

static ProcessState processes[MAX_HANDLED_PROCESS];

static inline QGloSurface *get_qsurface_from_client_drawable(GLState *state, ClientGLXDrawable client_drawable) {
    QGloSurface *qsurface;

    if(state->current_qsurface->client_drawable == client_drawable)
        return state->current_qsurface;

    QTAILQ_FOREACH(qsurface, &state->qsurfaces, next) {
        if (qsurface->client_drawable == client_drawable)
            return qsurface;
    }

    return NULL;
}

// This must always be called only on surfaces belonging to the current context
static inline void render_surface(QGloSurface *qsurface, int bpp, int stride, char *buffer)
{
    int w, h;
    if(!qsurface->ready)
	return;

    glo_surface_get_size(qsurface->surface, &w, &h);
//    for(x = 0 ; x < w ; x++)
//	for(y = 0 ; y < h ; y++){
//		char *p = buffer + y*stride + x*3;
//		int *pixel = (int*)p;
//		*pixel = (int)buffer;
//	}

    glo_surface_getcontents(qsurface->surface, stride, bpp, buffer);
}

// This must always be called only on surfaces belonging to the current context
static inline void resize_surface(ProcessState *process, QGloSurface *qsurface,
                                  int w, int h) {
    GLState *glstate = qsurface->glstate;
    GloSurface *old_surface = qsurface->surface;
    GloSurface *surface;

    DEBUGF("resize_start\n");

    surface = glo_surface_create(w, h, glstate->context);
    qsurface->surface = surface;

    glo_surface_destroy(old_surface);

    // Client doesnt know surface is new - need to MakeCurrent
    if(process->current_state == qsurface->glstate) {
        glo_surface_makecurrent(qsurface->surface);
    }
    else {
        DEBUGF("Error: Surface is not current! %p %p\n",
            process->current_state,
            process->current_state->current_qsurface);
        exit(1);
    }

    glstate->current_qsurface->ready = 1;
    DEBUGF( "resize_done\n");
}


void init_process_tab()
{
    memset(processes, 0, sizeof(processes));
}

#define ARG_TO_CHAR(x)                (char)(x)
#define ARG_TO_UNSIGNED_CHAR(x)       (unsigned char)(x)
#define ARG_TO_SHORT(x)               (short)(x)
#define ARG_TO_UNSIGNED_SHORT(x)      (unsigned short)(x)
#define ARG_TO_INT(x)                 (int)(x)
#define ARG_TO_UNSIGNED_INT(x)        (unsigned int)(x)
#define ARG_TO_FLOAT(x)               (*(float*)&(x))
#define ARG_TO_DOUBLE(x)              (*(double*)(x))

#include "server_stub.c"

//typedef void *ClientGLXDrawable;
static inline ClientGLXDrawable to_drawable(arg_t arg)
{
#ifdef TARGET_X86_64
    if (arg > (unsigned long) -1) {
        DEBUGF( "GLXDrawable too big for this implementation\n");
        exit(-1);
    }
#endif
    return (void *) (unsigned long) arg;
}

/* ---- */

#ifdef ALLOW_PBUFFER
GLXPbuffer get_association_fakepbuffer_pbuffer(
                ProcessState *process, ClientGLXDrawable fakepbuffer)
{
    int i;

    for (i = 0; i < MAX_ASSOC_SIZE &&
                    process->association_fakepbuffer_pbuffer[i].key; i ++)
        if ((ClientGLXDrawable)
                        process->association_fakepbuffer_pbuffer[i].key ==
                        fakepbuffer)
            return (GLXPbuffer)
                    process->association_fakepbuffer_pbuffer[i].value;

    return 0;
}

void set_association_fakepbuffer_pbuffer(ProcessState *process,
                ClientGLXDrawable fakepbuffer, GLXPbuffer pbuffer)
{
    int i;

    for (i = 0;
         i < MAX_ASSOC_SIZE &&
         process->association_fakepbuffer_pbuffer[i].key; i++)
        if ((ClientGLXDrawable)
                        process->association_fakepbuffer_pbuffer[i].key ==
                        fakepbuffer)
            break;

    if (i < MAX_ASSOC_SIZE) {
        process->association_fakepbuffer_pbuffer[i].key = (void *) fakepbuffer;
        process->association_fakepbuffer_pbuffer[i].value = (void *) pbuffer;
    } else
        DEBUGF( "MAX_ASSOC_SIZE reached\n");
}

void unset_association_fakepbuffer_pbuffer(ProcessState *process,
                ClientGLXDrawable fakepbuffer)
{
    int i;

    for (i = 0; i < MAX_ASSOC_SIZE &&
                    process->association_fakepbuffer_pbuffer[i].key; i++)
        if ((ClientGLXDrawable)
                        process->association_fakepbuffer_pbuffer[i].key ==
                        fakepbuffer) {
            memmove(&process->association_fakepbuffer_pbuffer[i],
                    &process->association_fakepbuffer_pbuffer[i + 1],
                    sizeof(Assoc) * (MAX_ASSOC_SIZE - 1 - i));
            return;
        }
}
#endif
/* ---- */

/* Bind a qsurface to a context (GLState) */
static void bind_qsurface(
                GLState *state,
                QGloSurface *qsurface)
{
    qsurface->glstate = state;

    QTAILQ_INSERT_HEAD(&state->qsurfaces, qsurface, next);

    state->current_qsurface = qsurface;
}

/* Make the appropriate qsurface current for a given client_drawable */
static int set_current_qsurface(
                 GLState *state,
                 ClientGLXDrawable client_drawable) {
    QGloSurface *qsurface;

    if(state->current_qsurface && state->current_qsurface->client_drawable == client_drawable)
        return 1;

    QTAILQ_FOREACH(qsurface, &state->qsurfaces, next) {
        if(qsurface->client_drawable == client_drawable) {
            state->current_qsurface = qsurface;
            return 1;
        }
    }

    state->current_qsurface = NULL;

    return 0;
}

static int get_server_texture(ProcessState *process,
                              unsigned int client_texture)
{
    unsigned int server_texture = 0;

    if (client_texture < 32768) {
        server_texture = process->current_state->tabTextures[client_texture];
    } else {
        DEBUGF( "invalid texture name %d\n", client_texture);
    }
    return server_texture;
}

static int get_server_buffer(ProcessState *process,
                             unsigned int client_buffer)
{
    unsigned int server_buffer = 0;

    if (client_buffer < 32768) {
        server_buffer = process->current_state->tabBuffers[client_buffer];
    } else {
        DEBUGF( "invalid buffer name %d\n", client_buffer);
    }
    return server_buffer;
}


static int get_server_list(ProcessState *process, unsigned int client_list)
{
    unsigned int server_list = 0;

    if (client_list < 32768) {
        server_list = process->current_state->tabLists[client_list];
    } else {
        DEBUGF( "invalid list name %d\n", client_list);
    }
    return server_list;
}

const GLXFBConfig *get_fbconfig(ProcessState *process, int client_fbconfig)
{
    int i;
    int nbtotal = 0;

    for (i = 0; i < process->nfbconfig; i++) {
        assert(client_fbconfig >= 1 + nbtotal);
        if (client_fbconfig <= nbtotal + process->fbconfigs_max[i]) {
            return &process->fbconfigs[i][client_fbconfig - 1 - nbtotal];
        }
        nbtotal += process->fbconfigs_max[i];
    }
    return 0;
}

static int glXChooseVisualFunc(const int *attrib_list)
{
    if (attrib_list == NULL)
        return 0;

    int formatFlags = glo_flags_get_from_glx(attrib_list, True);
    int i;
    int bestConfig = 0;
    int bestScore = -1;

    for (i=0;i<DIM(FBCONFIGS);i++) {
      int score = glo_flags_score(formatFlags, FBCONFIGS[i].formatFlags);
      if (bestScore < 0 || score<=bestScore) {
        bestScore = score;
        bestConfig = i;
      }
    }

    if (bestScore > 0)
      DEBUGF( "Got format flags %d but we couldn't find an exactly matching config, chose %d\n", formatFlags, bestConfig);
    DEBUGF( "glXChooseVisualFunc chose %d, format %d\n", bestConfig, FBCONFIGS[bestConfig].formatFlags);
    return bestConfig;
}

static int glXGetConfigFunc(int visualid, int attrib, int *value) {
  const GLXFBConfig *config = &FBCONFIGS[0]; // default

  if (visualid>=0 && visualid<DIM(FBCONFIGS))
    config = &FBCONFIGS[visualid];
  else
    DEBUGF( "Unknown visual ID %d\n", visualid);

  int v = glo_get_glx_from_flags(config->formatFlags, attrib);
  if (value) *value = v;
  return 0;
}

static const GLXFBConfig * glXGetFBConfigsFunc(int screen, int *nelements) {
  *nelements = DIM(FBCONFIGS);
  return &FBCONFIGS[0];
}

static int glXGetFBConfigAttribFunc(const GLXFBConfig *fbconfig, int attrib, int *value) {
   // TODO other enums - see http://www.opengl.org/sdk/docs/man/xhtml/glXGetFBConfigAttrib.xml

   int v = glo_get_glx_from_flags(fbconfig->formatFlags, attrib);
   if (value) *value = v;
   return 0;
}

static const GLXFBConfig *glXChooseFBConfigFunc(int screen, const int *attrib_list, int *nelements) {
  if (attrib_list != NULL) {
    int formatFlags = glo_flags_get_from_glx(attrib_list, False);
    int i;
    int bestConfig = 0;
    int bestScore = -1;

    for (i=0;i<DIM(FBCONFIGS);i++) {
      int score = glo_flags_score(formatFlags, FBCONFIGS[i].formatFlags);
      if (bestScore < 0 || score<=bestScore) {
        bestScore = score;
        bestConfig = i;
      }
    }

    if (bestScore > 0)
      DEBUGF( "Got format flags %d but we couldn't find an exactly matching config, chose %d\n", formatFlags, bestConfig);
    DEBUGF( "glXChooseVisualFunc chose %d, format %d\n", bestConfig, FBCONFIGS[bestConfig].formatFlags);
    if (nelements) *nelements=1;
    return &FBCONFIGS[bestConfig];
  }

  if (nelements) *nelements=0;
  return 0;
}

static void do_glClientActiveTextureARB(int texture)
{
    GET_EXT_PTR_NO_FAIL(void, glClientActiveTextureARB, (int));

    if (ptr_func_glClientActiveTextureARB) {
        ptr_func_glClientActiveTextureARB(texture);
    }
}

static void destroy_gl_state(GLState *state)
{
    int i;
    QGloSurface *qsurface;

    QTAILQ_FOREACH(qsurface, &state->qsurfaces, next) {
        glo_surface_destroy(qsurface->surface);
        QTAILQ_REMOVE(&state->qsurfaces, qsurface, next);
    }
// FIXMEIM free?
        
//    if (state->context)
//      glo_context_destroy(state->context);

    if (state->vertexPointer)
        qemu_free(state->vertexPointer);
    if (state->normalPointer)
        qemu_free(state->normalPointer);
    if (state->indexPointer)
        qemu_free(state->indexPointer);
    if (state->colorPointer)
        qemu_free(state->colorPointer);
    if (state->secondaryColorPointer)
        qemu_free(state->secondaryColorPointer);
    for (i = 0; i < NB_MAX_TEXTURES; i++) {
        if (state->texCoordPointer[i])
            qemu_free(state->texCoordPointer[i]);
    }
    for (i = 0; i < MY_GL_MAX_VERTEX_ATTRIBS_ARB; i++) {
        if (state->vertexAttribPointer[i])
            qemu_free(state->vertexAttribPointer[i]);
    }
    for (i = 0; i < MY_GL_MAX_VERTEX_ATTRIBS_NV; i++) {
        if (state->vertexAttribPointerNV[i])
            qemu_free(state->vertexAttribPointerNV[i]);
    }
    if (state->weightPointer)
        qemu_free(state->weightPointer);
    if (state->matrixIndexPointer)
        qemu_free(state->matrixIndexPointer);
    if (state->fogCoordPointer)
        qemu_free(state->fogCoordPointer);
    for (i = 0; i < MY_GL_MAX_VARIANT_POINTER_EXT; i++) {
        if (state->variantPointerEXT[i])
            qemu_free(state->variantPointerEXT[i]);
    }
    if (state->interleavedArrays)
        qemu_free(state->interleavedArrays);
    if (state->elementPointerATI)
        qemu_free(state->elementPointerATI);
}

static void init_gl_state(GLState *state)
{
    state->textureAllocator = &state->ownTextureAllocator;
    state->tabTextures = state->ownTabTextures;
    state->bufferAllocator = &state->ownBufferAllocator;
    state->tabBuffers = state->ownTabBuffers;
    state->listAllocator = &state->ownListAllocator;
    state->tabLists = state->ownTabLists;
}

/*
 * Translate the nth element of list from type to GLuint.
 */
static GLuint translate_id(GLsizei n, GLenum type, const GLvoid *list)
{
    GLbyte *bptr;
    GLubyte *ubptr;
    GLshort *sptr;
    GLushort *usptr;
    GLint *iptr;
    GLuint *uiptr;
    GLfloat *fptr;

    switch (type) {
    case GL_BYTE:
        bptr = (GLbyte *) list;
        return (GLuint) *(bptr + n);
    case GL_UNSIGNED_BYTE:
        ubptr = (GLubyte *) list;
        return (GLuint) *(ubptr + n);
    case GL_SHORT:
        sptr = (GLshort *) list;
        return (GLuint) *(sptr + n);
    case GL_UNSIGNED_SHORT:
        usptr = (GLushort *) list;
        return (GLuint) *(usptr + n);
    case GL_INT:
        iptr = (GLint *) list;
        return (GLuint) *(iptr + n);
    case GL_UNSIGNED_INT:
        uiptr = (GLuint *) list;
        return (GLuint) *(uiptr + n);
    case GL_FLOAT:
        fptr = (GLfloat *) list;
        return (GLuint) *(fptr + n);
    case GL_2_BYTES:
        ubptr = ((GLubyte *) list) + 2 * n;
        return (GLuint) *ubptr * 256 + (GLuint) *(ubptr + 1);
    case GL_3_BYTES:
        ubptr = ((GLubyte *) list) + 3 * n;
        return (GLuint) *ubptr * 65536 + (GLuint) *(ubptr + 1) * 256 +
            (GLuint) *(ubptr + 2);
    case GL_4_BYTES:
        ubptr = ((GLubyte *) list) + 4 * n;
        return (GLuint) *ubptr * 16777216 + (GLuint) *(ubptr + 1) * 65536 +
            (GLuint) *(ubptr + 2) * 256 + (GLuint) *(ubptr + 3);
  //FIXMEIM (use better constants)
    default:
        return 0;
    }
}

GLState *_create_context(ProcessState *process, int fake_ctxt, int fake_shareList)
{
    process->glstates =
        qemu_realloc(process->glstates,
                (process->nb_states + 1) * sizeof(GLState *));
    process->glstates[process->nb_states] = qemu_malloc(sizeof(GLState));
    memset(process->glstates[process->nb_states], 0, sizeof(GLState));
    process->glstates[process->nb_states]->ref = 1;
    process->glstates[process->nb_states]->fake_ctxt = fake_ctxt;
    process->glstates[process->nb_states]->fake_shareList = fake_shareList;
    init_gl_state(process->glstates[process->nb_states]);
    if (fake_shareList) {
        int i;

        for (i = 0; i < process->nb_states; i++) {
            if (process->glstates[i]->fake_ctxt == fake_shareList) {
                process->glstates[i]->ref++;
                process->glstates[process->nb_states]->textureAllocator =
                    process->glstates[i]->textureAllocator;
                process->glstates[process->nb_states]->tabTextures =
                    process->glstates[i]->tabTextures;
                process->glstates[process->nb_states]->bufferAllocator =
                    process->glstates[i]->bufferAllocator;
                process->glstates[process->nb_states]->tabBuffers =
                    process->glstates[i]->tabBuffers;
                process->glstates[process->nb_states]->listAllocator =
                    process->glstates[i]->listAllocator;
                process->glstates[process->nb_states]->tabLists =
                    process->glstates[i]->tabLists;
                break;
            }
        }
    }
    process->nb_states++;
    return process->glstates[process->nb_states-1];
}

GLState *get_glstate_for_fake_ctxt(ProcessState *process, int fake_ctxt)
{
  int i;
  for (i = 0; i < process->nb_states; i++)
    if (process->glstates[i]->fake_ctxt == fake_ctxt)
      return process->glstates[i];
  return 0;
}

void disconnect(ProcessState *process)
{
    int i;
#ifdef ALLOW_PBUFFER
    GET_EXT_PTR(void, glXDestroyPbuffer, (Display *, GLXPbuffer));
    for (i = 0; i < MAX_ASSOC_SIZE &&
                    process->association_fakepbuffer_pbuffer[i].key; i ++) {
        GLXPbuffer pbuffer = (GLXPbuffer)
                process->association_fakepbuffer_pbuffer[i].value;
        ptr_func_glXDestroyPbuffer(dpy, pbuffer);
    }
#endif
    for (i = 0; i < process->nb_states; i++) {
        destroy_gl_state(process->glstates[i]);
        qemu_free(process->glstates[i]);
    }
    destroy_gl_state(&process->default_state);
    qemu_free(process->glstates);

    if (process->cmdbuf)
        qemu_free(process->cmdbuf);

    for (i = 0; &processes[i] != process; i ++);
    memmove(&processes[i], &processes[i + 1],
                    (MAX_HANDLED_PROCESS - 1 - i) * sizeof(ProcessState));
}

static const int beginend_allowed[GL_N_CALLS] = {
#undef MAGIC_MACRO
#define MAGIC_MACRO(name) [name ## _func] = 1,
#include "gl_beginend.h"
};

ProcessStruct *vmgl_context_switch(pid_t pid, int switch_gl_context)
{
    ProcessState *process = NULL;
    int i;

    /* Lookup a process stuct. If there isnt one associated with this pid
     * then we create one.
     * process->current_state contains info on which of the guests contexts is
     * current.
     */
    for (i = 0; i < MAX_HANDLED_PROCESS; i ++)
        if (processes[i].p.process_id == pid) {
            process = &processes[i];
            break;
        } else if (processes[i].p.process_id == 0) {
            process = &processes[i];
            memset(process, 0, sizeof(ProcessState));
            process->p.process_id = pid;
            init_gl_state(&process->default_state);
            process->current_state = &process->default_state;
#if 0 //GW
            process->dpy = parent_dpy;
#endif
            break;
        }

    if (process == NULL) {
        DEBUGF( "Too many processes !\n");
        exit(-1);
    }

    if(switch_gl_context) {
//        DEBUGF( "Ctx switch: pid: %d    %08x %08x %08x\n", pid,
//                process->dpy, process->current_state->drawable,
//                process->current_state->context);
        if(process->current_state->current_qsurface)
            glo_surface_makecurrent(process->current_state->current_qsurface->surface);
        else
            glo_surface_makecurrent(0); // FIXMEIM - should never happen
    }

    return (ProcessStruct *)process; // Cast is ok due to struct defn.
}

int do_function_call(ProcessState *process, int func_number, arg_t *args, char *ret_string)
{
    union gl_ret_type ret;

    Signature *signature = (Signature *) tab_opengl_calls[func_number];
    int ret_type = signature->ret_type;

    ret.s = NULL;

    if (display_function_call) {
        DEBUGF( "[%d]> %s\n", process->p.process_id,
                tab_opengl_calls_name[func_number]);
    }

#if 0
    if(func_number != glXSwapBuffers_func &&
       func_number != glXMakeCurrent_func && 1/*tab_opengl_calls_name[func_number][2] == 'X'*/)
	DEBUGF( "[%d]> %s\n", process->p.process_id,
                tab_opengl_calls_name[func_number]);
#endif

    switch (func_number) {
    case -1:
        break;

    case _resize_surface_func:
        {
            ClientGLXDrawable client_drawable = to_drawable(args[0]);
            QGloSurface *qsurface = get_qsurface_from_client_drawable(
                                        process->current_state, client_drawable);

            // We have to assume the current context here
            // since we assume that a drawable must belong to a specific context
            resize_surface(process, qsurface, (int)args[1], (int)args[2]);
            break;

        }

    case _render_surface_func:
        {
            ClientGLXDrawable client_drawable = to_drawable(args[0]);
            QGloSurface *qsurface = get_qsurface_from_client_drawable(
                                        process->current_state, client_drawable);
            int bpp    = (int)args[1];
            int stride = (int)args[2];
            char *render_buffer = (char*)args[3];

//            DEBUGF( "win: %08x stride: %d buf: %08x cl_dr: %08x qsurf: %08x\n", args[0], args[1], args[2], client_drawable, qsurface);

            // We have to assume the current context here
            // since we assume that a drawable must belong to a specific context
            render_surface(qsurface, bpp, stride, render_buffer);
            break;

        }

    case glXWaitGL_func:
        {
            glFinish(); //glXWaitGL();
            ret.i = 0;
            break;
        }

    case glXWaitX_func:
        {
            // FIXME GW Maybe we should just do this on the server?
            //glXWaitX();
            ret.i = 0;
            break;
        }

    case glXChooseVisual_func:
        {
            ret.i = glXChooseVisualFunc((int *) args[2]);
            break;
        }

    case glXQueryExtensionsString_func:
        {
            // No extensions for you...
            ret.s = "";//glXQueryExtensionsString(dpy, 0);
            break;
        }

    case glXQueryServerString_func:
        {
            switch (args[2]) {
            case GLX_VENDOR : ret.s = "QEmu"; break;
            case GLX_VERSION : ret.s = FGLX_VERSION_STRING; break;
            case GLX_EXTENSIONS : ret.s = ""; break;
            default: ret.s = 0;
            }
            //ret.s = glXQueryServerString(dpy, 0, args[2]);
            break;
        }

    case glXGetClientString_func:
        {
            switch (args[2]) {
            case GLX_VENDOR : ret.s = "QEmu"; break;
            case GLX_VERSION : ret.s = FGLX_VERSION_STRING; break;
            case GLX_EXTENSIONS : ret.s = ""; break;
            default: ret.s = 0;
            }
            //ret.s = glXGetClientString(dpy, args[1]);
            break;
        }

    case glXGetScreenDriver_func:
        {
            // FIXME GW What is this? not documented anywhere!!
            //GET_EXT_PTR(const char *, glXGetScreenDriver, (Display *, int));
            //ret.s = ptr_func_glXGetScreenDriver(dpy, 0);
            ret.s = "";
            break;
        }

    case glXGetDriverConfig_func:
        {
            // FIXME GW What is this? not documented anywhere!!
            //GET_EXT_PTR(const char *, glXGetDriverConfig, (const char *));
            //ret.s = ptr_func_glXGetDriverConfig((const char *) args[0]);
            ret.s = "";
            break;
        }

    case glXCreateContext_func:
        {
            int visualid = (int) args[1];
            int fake_shareList = (int) args[2];

            if (1 || display_function_call)
                DEBUGF( "visualid=%d, fake_shareList=%d\n", visualid,
                        fake_shareList);

            GLState *shareListState = get_glstate_for_fake_ctxt(process, fake_shareList);
            int fake_ctxt = ++process->next_available_context_number;

            ret.i = fake_ctxt;

            // Work out format flags from visual id
            int formatFlags = GLO_FF_DEFAULT;
            if (visualid>=0 && visualid<DIM(FBCONFIGS))
              formatFlags = FBCONFIGS[visualid].formatFlags;

            GLState *state = _create_context(process, fake_ctxt, fake_shareList);
            state->context = glo_context_create(formatFlags,
                                                (GloContext*)shareListState?shareListState->context:0);

            DEBUGF( " created context %p for %08x\n", state, fake_ctxt);
            break;
        }


    case glXCreateNewContext_func:
        {
            int client_fbconfig = args[1];

            ret.i = 0;
            const GLXFBConfig *fbconfig = get_fbconfig(process, client_fbconfig);

            if (fbconfig) {
                int fake_shareList = args[3];
                GLState *shareListState = get_glstate_for_fake_ctxt(process, fake_shareList);

                process->next_available_context_number++;
                int fake_ctxt = process->next_available_context_number;
                ret.i = fake_ctxt;

                GLState *state = _create_context(process, fake_ctxt, fake_shareList);
                state->context = glo_context_create(fbconfig->formatFlags,
                                                    shareListState?shareListState->context:0); // FIXME GW get from fbconfig
            }
            break;
        }

    case glXCopyContext_func:
        {
          DEBUGF( " glXCopyContext not supported (does anything use it?)\n");
            break;
        }

    case glXDestroyContext_func:
        {
            int fake_ctxt = (int) args[1];

            if (display_function_call)
                DEBUGF( "fake_ctxt=%d\n", fake_ctxt);

            int i;
            for (i = 0; i < process->nb_states; i ++) {
                if (process->glstates[i]->fake_ctxt == fake_ctxt) {
                    // this was our GLState...
                    process->current_state = &process->default_state;

                    int fake_shareList =
                        process->glstates[i]->fake_shareList;
                    process->glstates[i]->ref--;
                    if (process->glstates[i]->ref == 0) {
                        DEBUGF(
                                "destroy_gl_state fake_ctxt = %d\n",
                                process->glstates[i]->fake_ctxt);
                        destroy_gl_state(process->glstates[i]);
                        qemu_free(process->glstates[i]);
                        memmove(&process->glstates[i],
                                &process->glstates[i + 1],
                                (process->nb_states - i - 1) *
                                sizeof(GLState *));
                        process->nb_states --;
                    }

                    if (fake_shareList) {
                        for (i = 0; i < process->nb_states; i++) {
                            if (process->glstates[i]->fake_ctxt ==
                                fake_shareList) {
                                process->glstates[i]->ref--;
                                if (process->glstates[i]->ref == 0) {
                                    DEBUGF(
                                            "destroy_gl_state fake_ctxt = %d\n",
                                            process->glstates[i]->
                                            fake_ctxt);
                                    destroy_gl_state(process->
                                                     glstates[i]);
                                    qemu_free(process->glstates[i]);
                                    memmove(&process->glstates[i],
                                            &process->glstates[i + 1],
                                            (process->nb_states - i - 1) *
                                            sizeof(GLState *));
                                    process->nb_states --;
                                }
                                break;
                            }
                        }
                    }

                    break;
                }
            }
            break;
        }

    case glXQueryVersion_func:
        {
            int *major = (int *) args[1];
            int *minor = (int *) args[2];
            //ret.i = glXQueryVersion(dpy, (int *) args[1], (int *) args[2]);
            if (major) *major=FGLX_VERSION_MAJOR;
            if (minor) *minor=FGLX_VERSION_MINOR;
            ret.i = True;
            break;
        }

    case glGetString_func:
        {
            ret.s = (char *) glGetString(args[0]);
            break;
        }

    case glXMakeCurrent_func:
        {
            ClientGLXDrawable client_drawable = to_drawable(args[1]);
            int fake_ctxt = (int) args[2];
            GLState *glstate = NULL;

//            DEBUGF( "Makecurrent: fake_ctx=%d client_drawable=%08x\n", fake_ctxt, client_drawable);

            if (client_drawable == 0 && fake_ctxt == 0) {
                /* Release context */
                if(process->current_state->current_qsurface)
                    process->current_state->current_qsurface->ref--;
                process->current_state = &process->default_state;

//                DEBUGF( " --release\n");
                glo_surface_makecurrent(0);
            } else { /* Lookup GLState struct for this context */
                glstate = get_glstate_for_fake_ctxt(process, fake_ctxt);
                if (!glstate) {
                    DEBUGF( " --invalid fake_ctxt (%d)!\n", fake_ctxt);
                } else {
                    if(!set_current_qsurface(glstate, client_drawable)) {
                       // If there is no surface, create one.
                       QGloSurface *qsurface = calloc(1, sizeof(QGloSurface));
                       qsurface->surface = glo_surface_create(4, 4,
                                                              glstate->context);
                       qsurface->client_drawable = client_drawable;
                       qsurface->ref = 1;

                       bind_qsurface(glstate, qsurface);
//                       DEBUGF( " --Client drawable not found, create new surface: %16x %16lx\n", (unsigned int)qsurface, (unsigned long int)client_drawable);

                    }
                    else {
//                       DEBUGF( " --Client drawable found, using surface: %16x %16lx\n", (unsigned int)glstate->current_qsurface, (unsigned long int)client_drawable);
                    }

                    process->current_state = glstate;

                    ret.i = glo_surface_makecurrent(glstate->current_qsurface->surface);
                }
            }
            break;
        }

    case glXSwapBuffers_func:
        {
            // Does nothing - window data is copied via render_surface()
            break;
        }
    case glXIsDirect_func:
        {
            // int fake_ctxt = (int) args[1];

            // Does this go direct and skip the X server? We'll just say
            // yes for now.
            ret.c = True;

            break;
        }

    case glXGetConfig_func:
        {
            int visualid = args[1];
            ret.i = glXGetConfigFunc(visualid, args[2], (int *) args[3]);
            break;
        }

    case glXGetConfig_extended_func:
        {
            int visualid = args[1];
            int n = args[2];
            int i;
            int *attribs = (int *) args[3];
            int *values = (int *) args[4];
            int *res = (int *) args[5];

            for (i = 0; i < n; i++) {
                res[i] = glXGetConfigFunc(visualid, attribs[i], &values[i]);
            }
            break;
        }

    case glXUseXFont_func:
        {
            /* implementation is client-side only :-) */
            break;
        }

    case glXQueryExtension_func:
        {
            int *errorBase = (int *) args[1];
            int *eventBase = (int *) args[2];
            if (errorBase) *errorBase = 0; /* FIXME GW */
            if (eventBase) *eventBase = 0; /* FIXME GW */
            ret.i = True;
            break;
        }

    case glXChooseFBConfig_func:
        {
            if (process->nfbconfig == MAX_FBCONFIG) {
                *(int *) args[3] = 0;
                ret.i = 0;
            } else {
                int *attrib_list = (int *) args[2];
		while (*attrib_list != None) {
			if(*attrib_list == GLX_DOUBLEBUFFER) {
				DEBUGF( "Squashing doublebuffered visual\n");
				*(attrib_list+1) = False;
			}
			attrib_list += 2;
		}
                const GLXFBConfig *fbconfigs =
                    glXChooseFBConfigFunc(args[1], (int *) args[2], (int *) args[3]);
                if (fbconfigs) {
                    process->fbconfigs[process->nfbconfig] = fbconfigs;
                    process->fbconfigs_max[process->nfbconfig] =
                        *(int *) args[3];
                    process->nfbconfig++;
                    ret.i = 1 + process->nfbconfig_total;
                    process->nfbconfig_total +=
                        process->fbconfigs_max[process->nfbconfig];
                } else {
                    ret.i = 0;
                }
            }
            break;
        }
#ifdef ALLOW_PBUFFER
    case glXChooseFBConfigSGIX_func:
        {
            GET_EXT_PTR(GLXFBConfigSGIX *, glXChooseFBConfigSGIX,
                        (Display *, int, int *, int *));
            if (process->nfbconfig == MAX_FBCONFIG) {
                *(int *) args[3] = 0;
                ret.i = 0;
            } else {
                GLXFBConfigSGIX *fbconfigs =
                    ptr_func_glXChooseFBConfigSGIX(dpy, args[1],
                                                   (int *) args[2],
                                                   (int *) args[3]);
                if (fbconfigs) {
                    process->fbconfigs[process->nfbconfig] = fbconfigs;
                    process->fbconfigs_max[process->nfbconfig] =
                        *(int *) args[3];
                    process->nfbconfig++;
                    ret.i = 1 + process->nfbconfig_total;
                    process->nfbconfig_total +=
                        process->fbconfigs_max[process->nfbconfig];
                } else {
                    ret.i = 0;
                }
            }
            break;
        }
#endif
    case glXGetFBConfigs_func:
        {
            if (process->nfbconfig == MAX_FBCONFIG) {
                *(int *) args[2] = 0;
                ret.i = 0;
            } else {
                const GLXFBConfig *fbconfigs =
                    glXGetFBConfigsFunc(args[1], (int *) args[2]);
                if (fbconfigs) {
                    process->fbconfigs[process->nfbconfig] = fbconfigs;
                    process->fbconfigs_max[process->nfbconfig] =
                        *(int *) args[2];
                    process->nfbconfig++;
                    ret.i = 1 + process->nfbconfig_total;
                    process->nfbconfig_total +=
                        process->fbconfigs_max[process->nfbconfig];
                } else {
                    ret.i = 0;
                }
            }
            break;
        }
#ifdef ALLOW_PBUFFER
    case glXCreatePbuffer_func:
        {
            GET_EXT_PTR(GLXPbuffer, glXCreatePbuffer,
                        (Display *, GLXFBConfig, int *));
            int client_fbconfig = args[1];

            ret.i = 0;
            GLXFBConfig fbconfig = get_fbconfig(process, client_fbconfig);

            if (fbconfig) {
                GLXPbuffer pbuffer =
                    ptr_func_glXCreatePbuffer(dpy, fbconfig, (int *) args[2]);
                DEBUGF( "glXCreatePbuffer --> %x\n", (int) pbuffer);
                if (pbuffer) {
                    ClientGLXDrawable fake_pbuffer = to_drawable(
                                    ++ process->next_available_pbuffer_number);

                    set_association_fakepbuffer_pbuffer(
                                    process, fake_pbuffer, pbuffer);
                    DEBUGF(
                            "set_association_fakepbuffer_pbuffer(%p, %x)\n",
                            fake_pbuffer, (int) (long) pbuffer);
                    ret.i = (int) (long) fake_pbuffer;
                }
            }
            break;
        }

    case glXCreateGLXPbufferSGIX_func:
        {
            GET_EXT_PTR(GLXPbufferSGIX, glXCreateGLXPbufferSGIX,
                        (Display *, GLXFBConfig, int, int, int *));
            int client_fbconfig = args[1];

            ret.i = 0;
            GLXFBConfig fbconfig = get_fbconfig(process, client_fbconfig);

            if (fbconfig) {
                GLXPbufferSGIX pbuffer = ptr_func_glXCreateGLXPbufferSGIX(
                                dpy, fbconfig,
                                args[2], args[3], (int *) args[4]);
                if (pbuffer) {
                    ClientGLXDrawable fake_pbuffer = to_drawable(
                                    ++ process->next_available_pbuffer_number);

                    set_association_fakepbuffer_pbuffer(
                                    process, fake_pbuffer, pbuffer);
                    ret.i = (int) (long) fake_pbuffer;
                }
            }
            break;
        }

    case glXDestroyPbuffer_func:
        {
            GET_EXT_PTR(void, glXDestroyPbuffer, (Display *, GLXPbuffer));
            ClientGLXDrawable fake_pbuffer = to_drawable(args[1]);

            if (display_function_call)
                DEBUGF( "fake_pbuffer=%p\n", fake_pbuffer);

            GLXPbuffer pbuffer = get_association_fakepbuffer_pbuffer(
                            process, fake_pbuffer);
            if (pbuffer == 0) {
                DEBUGF( "invalid fake_pbuffer (%p) !\n",
                                fake_pbuffer);
            } else {
                if (!is_gl_vendor_ati(dpy))
                    ptr_func_glXDestroyPbuffer(dpy, pbuffer);
                unset_association_fakepbuffer_pbuffer(process, fake_pbuffer);
            }
            break;
        }

    case glXDestroyGLXPbufferSGIX_func:
        {
            GET_EXT_PTR(void, glXDestroyGLXPbufferSGIX,
                        (Display *, GLXPbuffer));
            ClientGLXDrawable fake_pbuffer = to_drawable(args[1]);

            if (display_function_call)
                DEBUGF( "fake_pbuffer=%p\n", fake_pbuffer);

            GLXPbuffer pbuffer = get_association_fakepbuffer_pbuffer(
                            process, fake_pbuffer);
            if (pbuffer == 0) {
                DEBUGF( "invalid fake_pbuffer (%p)  !\n",
                        fake_pbuffer);
            } else {
                if (!is_gl_vendor_ati(dpy))
                    ptr_func_glXDestroyGLXPbufferSGIX(dpy, pbuffer);
                unset_association_fakepbuffer_pbuffer(process, fake_pbuffer);
            }
            break;
        }
    case glXQueryGLXPbufferSGIX_func:
        {
            GET_EXT_PTR(int, glXQueryGLXPbufferSGIX,
                        (Display *, GLXFBConfigSGIX, int, int *));
            int client_fbconfig = args[1];

            ret.i = 0;
            GLXFBConfig fbconfig = get_fbconfig(process, client_fbconfig);

            if (fbconfig)
                ret.i = ptr_func_glXQueryGLXPbufferSGIX(dpy,
                                (GLXFBConfigSGIX) fbconfig,
                                args[2], (int *) args[3]);
            break;
        }
    case glXBindTexImageATI_func:
        {
            GET_EXT_PTR(void, glXBindTexImageATI,
                        (Display *, GLXPbuffer, int));
            ClientGLXDrawable fake_pbuffer = to_drawable(args[1]);

            if (display_function_call)
                DEBUGF( "fake_pbuffer=%p\n",
                                fake_pbuffer);

            GLXPbuffer pbuffer = get_association_fakepbuffer_pbuffer(
                                process, fake_pbuffer);
            if (pbuffer == 0) {
                DEBUGF(
                        "glXBindTexImageATI : invalid fake_pbuffer (%p) !\n",
                        fake_pbuffer);
            } else {
                ptr_func_glXBindTexImageATI(dpy, pbuffer, args[2]);
            }
            break;
        }

    case glXReleaseTexImageATI_func:
        {
            GET_EXT_PTR(void, glXReleaseTexImageATI,
                        (Display *, GLXPbuffer, int));
            ClientGLXDrawable fake_pbuffer = to_drawable(args[1]);

            if (display_function_call)
                DEBUGF( "fake_pbuffer=%d\n",
                                (int) (long) fake_pbuffer);

            GLXPbuffer pbuffer = get_association_fakepbuffer_pbuffer(
                            process, fake_pbuffer);
            if (pbuffer == 0) {
                DEBUGF(
                        "glXReleaseTexImageATI : invalid fake_pbuffer (%d) !\n",
                        (int) (long) fake_pbuffer);
            } else {
                ptr_func_glXReleaseTexImageATI(dpy, pbuffer, args[2]);
            }
            break;
        }

    case glXBindTexImageARB_func:
        {
            GET_EXT_PTR(Bool, glXBindTexImageARB,
                        (Display *, GLXPbuffer, int));
            ClientGLXDrawable fake_pbuffer = to_drawable(args[1]);

            if (display_function_call)
                DEBUGF( "fake_pbuffer=%p\n", fake_pbuffer);

            GLXPbuffer pbuffer = get_association_fakepbuffer_pbuffer(
                            process, fake_pbuffer);
            if (pbuffer == 0) {
                DEBUGF(
                        "glXBindTexImageARB : invalid fake_pbuffer (%p) !\n",
                        fake_pbuffer);
                ret.i = 0;
            } else {
                ret.i = ptr_func_glXBindTexImageARB(dpy, pbuffer, args[2]);
            }
            break;
        }

    case glXReleaseTexImageARB_func:
        {
            GET_EXT_PTR(Bool, glXReleaseTexImageARB,
                        (Display *, GLXPbuffer, int));
            ClientGLXDrawable fake_pbuffer = to_drawable(args[1]);

            if (display_function_call)
                DEBUGF( "fake_pbuffer=%p\n", fake_pbuffer);

            GLXPbuffer pbuffer = get_association_fakepbuffer_pbuffer(
                            process, fake_pbuffer);
            if (pbuffer == 0) {
                DEBUGF(
                        "glXReleaseTexImageARB : invalid fake_pbuffer (%p) !\n",
                        fake_pbuffer);
                ret.i = 0;
            } else {
                ret.i =
                    ptr_func_glXReleaseTexImageARB(dpy, pbuffer, args[2]);
            }
            break;
        }
#endif
    case glXGetFBConfigAttrib_func:
        {
            int client_fbconfig = args[1];

            ret.i = 0;
            const GLXFBConfig *fbconfig = get_fbconfig(process, client_fbconfig);

            if (fbconfig)
                ret.i =
                    glXGetFBConfigAttribFunc(fbconfig, args[2], (int *) args[3]);
            break;
        }

    case glXGetFBConfigAttrib_extended_func:
        {
            int client_fbconfig = args[1];
            int n = args[2];
            int i;
            int *attribs = (int *) args[3];
            int *values = (int *) args[4];
            int *res = (int *) args[5];
            const GLXFBConfig *fbconfig = get_fbconfig(process, client_fbconfig);

            for (i = 0; i < n; i++) {
                if (fbconfig) {
                    res[i] =
                        glXGetFBConfigAttribFunc(fbconfig, attribs[i], &values[i]);
                } else {
                    res[i] = 0;
                }
            }
            break;
        }
#ifdef ALLOW_PBUFFER
    case glXGetFBConfigAttribSGIX_func:
        {
            GET_EXT_PTR(int, glXGetFBConfigAttribSGIX,
                        (Display *, GLXFBConfigSGIX, int, int *));
            int client_fbconfig = args[1];

            ret.i = 0;
            GLXFBConfig fbconfig = get_fbconfig(process, client_fbconfig);

            if (fbconfig)
                ret.i =
                    ptr_func_glXGetFBConfigAttribSGIX(dpy,
                                                      (GLXFBConfigSGIX)
                                                      fbconfig, args[2],
                                                      (int *) args[3]);
            break;
        }
#endif
    case glXQueryContext_func:
        {
            DEBUGF( "glXQueryContext not implemented\n");
            ret.i = 0;
#if 0 //GW
            GET_EXT_PTR(int, glXQueryContext,
                        (Display *, GLXContext, int, int *));
            int fake_ctxt = (int) args[1];

            if (display_function_call)
                DEBUGF( "fake_ctx=%i\n", fake_ctxt);
            GLXContext ctxt =
                get_association_fakecontext_glxcontext(process, fake_ctxt);
            if (ctxt == NULL) {
                DEBUGF( "invalid fake_ctxt (%i) !\n", fake_ctxt);
                ret.i = 0;
            } else {
                ret.i =
                    ptr_func_glXQueryContext(dpy, ctxt, args[2],
                                             (int *) args[3]);
            }
#endif
            break;
        }

    case glXQueryDrawable_func:
        {
            // TODO GW one of:
            // GLX_WIDTH, GLX_HEIGHT, GLX_PRESERVED_CONTENTS, GLX_LARGEST_PBUFFER, GLX_FBCONFIG_ID
            DEBUGF( "FIXME: glXQueryDrawable not implemented\n");
            ret.i = 0;
#if 0 //GW
            GET_EXT_PTR(void, glXQueryDrawable,
                        (Display *, GLXDrawable, int, int *));
            ClientGLXDrawable client_drawable = to_drawable(args[1]);
            GLXDrawable drawable =
                    get_association_clientdrawable_serverdrawable(
                                    glstate, client_drawable);

            if (display_function_call)
                DEBUGF( "client_drawable=%p\n",
                                client_drawable);

            if (!drawable)
                DEBUGF( "invalid client_drawable (%p) !\n",
                                client_drawable);
            else
                ptr_func_glXQueryDrawable(dpy, drawable,
                                args[2], (int *) args[3]);
#endif
            break;
        }
#ifdef ALLOW_PBUFFER
    case glXCreateContextWithConfigSGIX_func:
        {
            printf("glXCreateContextWithConfigSGIX not implemented\n");
#if 0 //GW
            GET_EXT_PTR(GLXContext, glXCreateContextWithConfigSGIX,
                        (Display *, GLXFBConfigSGIX, int, GLXContext, int));
            int client_fbconfig = args[1];

            ret.i = 0;
            GLXFBConfig fbconfig = get_fbconfig(process, client_fbconfig);

            if (fbconfig) {
                GLXContext shareList = get_association_fakecontext_glxcontext(
                                process, (int) args[3]);
                process->next_available_context_number++;
                int fake_ctxt = process->next_available_context_number;
                GLXContext ctxt = ptr_func_glXCreateContextWithConfigSGIX(
                                dpy, (GLXFBConfigSGIX) fbconfig, args[2],
                                shareList, args[4]);
                set_association_fakecontext_glxcontext(
                                process, fake_ctxt, ctxt);
                ret.i = fake_ctxt;
            }
#endif
            break;
        }
#endif
    case glXGetVisualFromFBConfig_func:
        {
            int client_fbconfig = args[1];

            ret.i = 0;
            const GLXFBConfig *fbconfig = get_fbconfig(process, client_fbconfig);

            if (fbconfig) {
                // we tread visualid as the index into the fbconfigs array
                ret.i = &FBCONFIGS[0] - fbconfig;
                if (display_function_call)
                    DEBUGF( "visualid = %d\n", ret.i);
            }
            break;
        }
    case glXSwapIntervalSGI_func:
        {
            /*GET_EXT_PTR(int, glXSwapIntervalSGI, (int));
            ret.i = ptr_func_glXSwapIntervalSGI(args[0]);*/
            ret.i = 0;
            break;
        }

    case glXGetProcAddress_fake_func:
        {
//            if (display_function_call)
            //DEBUGF( "glXGetProcAddress %s  ", (char *) args[0]);
            ret.i = glo_getprocaddress((const GLubyte *) args[0]) != NULL;
            //   DEBUGF( " == %08x\n", ret.i);
            ret.i = 0;
            break;
        }

    case glXGetProcAddress_global_fake_func:
        {
            int nbElts = args[0];
            char *huge_buffer = (char *) args[1];
            char *result = (char *) args[2];
            int i;

            for (i = 0; i < nbElts; i++) {
                int len = strlen(huge_buffer);
                //DEBUGF( "glXGetProcAddress_global %s  ", (char *)huge_buffer);
                result[i] =
                    glo_getprocaddress((const GLubyte *) huge_buffer) !=
                    NULL;
                huge_buffer += len + 1;
            }
            break;
        }
/* Begin of texture stuff */
    case glBindTexture_func:
    case glBindTextureEXT_func:
        {
            int target = args[0];
            unsigned int client_texture = args[1];
            unsigned int server_texture;

            if (client_texture == 0) {
                glBindTexture(target, 0);
            } else {
                alloc_value(process->current_state->textureAllocator,
                            client_texture);
                server_texture =
                    process->current_state->tabTextures[client_texture];
                if (server_texture == 0) {
                    glGenTextures(1, &server_texture);
                    process->current_state->tabTextures[client_texture] =
                        server_texture;
                }
                glBindTexture(target, server_texture);
            }
            break;
        }

    case glGenTextures_fake_func:
        {
            GET_EXT_PTR(void, glGenTextures, (GLsizei n, GLuint *textures));
            int i;
            int n = args[0];
            unsigned int *clientTabTextures = qemu_malloc(n * sizeof(int));
            unsigned int *serverTabTextures = qemu_malloc(n * sizeof(int));

            alloc_range(process->current_state->textureAllocator, n,
                        clientTabTextures);

            ptr_func_glGenTextures(n, serverTabTextures);
            for (i = 0; i < n; i++) {
                process->current_state->tabTextures[clientTabTextures[i]] =
                    serverTabTextures[i];
            }

            qemu_free(clientTabTextures);
            qemu_free(serverTabTextures);
            break;
        }


    case glDeleteTextures_func:
        {
            GET_EXT_PTR(void, glDeleteTextures,
                        (GLsizei n, const GLuint *textures));
            int i;
            int n = args[0];
            unsigned int *clientTabTextures = (unsigned int *) args[1];

            delete_range(process->current_state->textureAllocator, n,
                         clientTabTextures);

            unsigned int *serverTabTextures = qemu_malloc(n * sizeof(int));

            for (i = 0; i < n; i++) {
                serverTabTextures[i] =
                    get_server_texture(process, clientTabTextures[i]);
            }
            ptr_func_glDeleteTextures(n, serverTabTextures);
            for (i = 0; i < n; i++) {
                process->current_state->tabTextures[clientTabTextures[i]] = 0;
            }
            qemu_free(serverTabTextures);
            break;
        }

    case glPrioritizeTextures_func:
        {
            GET_EXT_PTR(void, glPrioritizeTextures,
                        (GLsizei n, const GLuint *textures,
                         const GLclampf *priorities));

            int i;
            int n = args[0];
            unsigned int *textures = (unsigned int *) args[1];

            for (i = 0; i < n; i++) {
                textures[i] = get_server_texture(process, textures[i]);
            }
            ptr_func_glPrioritizeTextures(n, textures,
                                          (const GLclampf *) args[2]);
            break;
        }

    case glAreTexturesResident_func:
        {
            GET_EXT_PTR(void, glAreTexturesResident,
                        (GLsizei n, const GLuint *textures,
                         GLboolean *residences));
            int i;
            int n = args[0];
            unsigned int *textures = (unsigned int *) args[1];

            for (i = 0; i < n; i++) {
                textures[i] = get_server_texture(process, textures[i]);
            }
            ptr_func_glAreTexturesResident(n, textures,
                                           (GLboolean *) args[2]);
            break;
        }

    case glIsTexture_func:
    case glIsTextureEXT_func:
        {
            GET_EXT_PTR(GLboolean, glIsTexture, (GLuint texture));
            unsigned int client_texture = args[0];
            unsigned int server_texture =
                get_server_texture(process, client_texture);
            if (server_texture)
                ret.c = ptr_func_glIsTexture(server_texture);
            else
                ret.c = 0;
            break;
        }

    case glFramebufferTexture1DEXT_func:
        {
            GET_EXT_PTR(void, glFramebufferTexture1DEXT,
                        (int, int, int, int, int));
            unsigned int client_texture = args[3];
            unsigned int server_texture =
                get_server_texture(process, client_texture);
            if (server_texture)
                ptr_func_glFramebufferTexture1DEXT(args[0], args[1], args[2],
                                                   server_texture, args[4]);
            break;
        }

    case glFramebufferTexture2D_func:
        DEBUGF( "wooooot!\n");
    case glFramebufferTexture2DEXT_func:
        {
            GET_EXT_PTR(void, glFramebufferTexture2DEXT,
                        (int, int, int, int, int));
            unsigned int client_texture = args[3];
            unsigned int server_texture =
                get_server_texture(process, client_texture);
            if (server_texture)
                ptr_func_glFramebufferTexture2DEXT(args[0], args[1], args[2],
                                                   server_texture, args[4]);
            break;
        }

    case glFramebufferTexture3DEXT_func:
        {
            GET_EXT_PTR(void, glFramebufferTexture3DEXT,
                        (int, int, int, int, int, int));
            unsigned int client_texture = args[3];
            unsigned int server_texture =
                get_server_texture(process, client_texture);
            if (server_texture)
                ptr_func_glFramebufferTexture3DEXT(args[0], args[1], args[2],
                                                   server_texture, args[4],
                                                   args[5]);
            break;
        }
/* End of texture stuff */

/* Begin of list stuff */
    case glIsList_func:
        {
            unsigned int client_list = args[0];
            unsigned int server_list = get_server_list(process, client_list);

            if (server_list)
                ret.c = glIsList(server_list);
            else
                ret.c = 0;
            break;
        }

    case glDeleteLists_func:
        {
            int i;
            unsigned int first_client = args[0];
            int n = args[1];

            unsigned int first_server =
                get_server_list(process, first_client);
            for (i = 0; i < n; i++) {
                if (get_server_list(process, first_client + i) !=
                    first_server + i)
                    break;
            }
            if (i == n) {
                glDeleteLists(first_server, n);
            } else {
                for (i = 0; i < n; i++) {
                    glDeleteLists(get_server_list(process, first_client + i),
                                  1);
                }
            }

            for (i = 0; i < n; i++) {
                process->current_state->tabLists[first_client + i] = 0;
            }
            delete_consecutive_values(process->current_state->listAllocator,
                                      first_client, n);
            break;
        }

    case glGenLists_fake_func:
        {
            int i;
            int n = args[0];
            unsigned int server_first = glGenLists(n);

            if (server_first) {
                unsigned int client_first =
                    alloc_range(process->current_state->listAllocator, n,
                                NULL);
                for (i = 0; i < n; i++) {
                    process->current_state->tabLists[client_first + i] =
                        server_first + i;
                }
            }
            break;
        }

    case glNewList_func:
        {
            unsigned int client_list = args[0];
            int mode = args[1];

            alloc_value(process->current_state->listAllocator, client_list);
            unsigned int server_list = get_server_list(process, client_list);

            if (server_list == 0) {
                server_list = glGenLists(1);
                process->current_state->tabLists[client_list] = server_list;
            }
            glNewList(server_list, mode);
            break;
        }

    case glCallList_func:
        {
            unsigned int client_list = args[0];
            unsigned int server_list = get_server_list(process, client_list);

            glCallList(server_list);
            break;
        }

    case glCallLists_func:
        {
            int i;
            int n = args[0];
            int type = args[1];
            const GLvoid *lists = (const GLvoid *) args[2];
            int *new_lists = qemu_malloc(sizeof(int) * n);

            for (i = 0; i < n; i++) {
                new_lists[i] =
                    get_server_list(process, translate_id(i, type, lists));
            }
            glCallLists(n, GL_UNSIGNED_INT, new_lists);
            qemu_free(new_lists);
            break;
        }


/* End of list stuff */

/* Begin of buffer stuff */
    case glBindBufferARB_func:
        {
            GET_EXT_PTR(void, glBindBufferARB, (int, int));
            int target = args[0];
            unsigned int client_buffer = args[1];
            unsigned int server_buffer;

            if (client_buffer == 0) {
                ptr_func_glBindBufferARB(target, 0);
            } else {
                server_buffer = get_server_buffer(process, client_buffer);
                ptr_func_glBindBufferARB(target, server_buffer);
            }
            break;
        }

    case glGenBuffersARB_fake_func:
        {
            GET_EXT_PTR(void, glGenBuffersARB, (int, unsigned int *));
            int i;
            int n = args[0];
            unsigned int *clientTabBuffers = qemu_malloc(n * sizeof(int));
            unsigned int *serverTabBuffers = qemu_malloc(n * sizeof(int));

            alloc_range(process->current_state->bufferAllocator, n,
                        clientTabBuffers);

            ptr_func_glGenBuffersARB(n, serverTabBuffers);
            for (i = 0; i < n; i++) {
                process->current_state->tabBuffers[clientTabBuffers[i]] =
                    serverTabBuffers[i];
            }

            qemu_free(clientTabBuffers);
            qemu_free(serverTabBuffers);
            break;
        }


    case glDeleteBuffersARB_func:
        {
            GET_EXT_PTR(void, glDeleteBuffersARB, (int, int *));
            int i;
            int n = args[0];
            unsigned int *clientTabBuffers = (unsigned int *) args[1];

            delete_range(process->current_state->bufferAllocator, n,
                         clientTabBuffers);

            int *serverTabBuffers = qemu_malloc(n * sizeof(int));

            for (i = 0; i < n; i++) {
                serverTabBuffers[i] =
                    get_server_buffer(process, clientTabBuffers[i]);
            }
            ptr_func_glDeleteBuffersARB(n, serverTabBuffers);
            for (i = 0; i < n; i++) {
                process->current_state->tabBuffers[clientTabBuffers[i]] = 0;
            }
            qemu_free(serverTabBuffers);
            break;
        }

    case glIsBufferARB_func:
        {
            GET_EXT_PTR(int, glIsBufferARB, (int));
            unsigned int client_buffer = args[0];
            unsigned int server_buffer =
                get_server_buffer(process, client_buffer);
            if (server_buffer)
                ret.i = ptr_func_glIsBufferARB(server_buffer);
            else
                ret.i = 0;
            break;
        }

/* End of buffer stuff */

    case glShaderSourceARB_fake_func:
        {
            GET_EXT_PTR(void, glShaderSourceARB, (int, int, char **, void *));
            int size = args[1];
            int i;
            int acc_length = 0;
            GLcharARB **tab_prog = qemu_malloc(size * sizeof(GLcharARB *));
            int *tab_length = (int *) args[3];

            for (i = 0; i < size; i++) {
                tab_prog[i] = ((GLcharARB *) args[2]) + acc_length;
                acc_length += tab_length[i];
            }
            ptr_func_glShaderSourceARB(args[0], args[1], tab_prog,
                                       tab_length);
            qemu_free(tab_prog);
            break;
        }

    case glShaderSource_fake_func:
        {
            GET_EXT_PTR(void, glShaderSource, (int, int, char **, void *));
            int size = args[1];
            int i;
            int acc_length = 0;
            GLcharARB **tab_prog = qemu_malloc(size * sizeof(GLcharARB *));
            int *tab_length = (int *) args[3];

            for (i = 0; i < size; i++) {
                tab_prog[i] = ((GLcharARB *) args[2]) + acc_length;
                acc_length += tab_length[i];
            }
            ptr_func_glShaderSource(args[0], args[1], tab_prog, tab_length);
            qemu_free(tab_prog);
            break;
        }

    case glVertexPointer_fake_func:
        {
            int offset = args[0];
            int size = args[1];
            int type = args[2];
            int stride = args[3];
            int bytes_size = args[4];

            process->current_state->vertexPointerSize =
                MAX(process->current_state->vertexPointerSize,
                    offset + bytes_size);
            process->current_state->vertexPointer =
                qemu_realloc(process->current_state->vertexPointer,
                        process->current_state->vertexPointerSize);
            memcpy(process->current_state->vertexPointer + offset,
                   (void *) args[5], bytes_size);
            /* DEBUGF( "glVertexPointer_fake_func size=%d, type=%d,
             * stride=%d, byte_size=%d\n", size, type, stride, bytes_size); */
            glVertexPointer(size, type, stride,
                            process->current_state->vertexPointer);
            break;
        }

    case glNormalPointer_fake_func:
        {
            int offset = args[0];
            int type = args[1];
            int stride = args[2];
            int bytes_size = args[3];

            process->current_state->normalPointerSize =
                MAX(process->current_state->normalPointerSize,
                    offset + bytes_size);
            process->current_state->normalPointer =
                qemu_realloc(process->current_state->normalPointer,
                        process->current_state->normalPointerSize);
            memcpy(process->current_state->normalPointer + offset,
                   (void *) args[4], bytes_size);
            // DEBUGF( "glNormalPointer_fake_func type=%d, stride=%d, 
            // byte_size=%d\n", type, stride, bytes_size);
            glNormalPointer(type, stride,
                            process->current_state->normalPointer);
            break;
        }

    case glIndexPointer_fake_func:
        {
            int offset = args[0];
            int type = args[1];
            int stride = args[2];
            int bytes_size = args[3];

            process->current_state->indexPointerSize =
                MAX(process->current_state->indexPointerSize,
                    offset + bytes_size);
            process->current_state->indexPointer =
                qemu_realloc(process->current_state->indexPointer,
                        process->current_state->indexPointerSize);
            memcpy(process->current_state->indexPointer + offset,
                   (void *) args[4], bytes_size);
            // DEBUGF( "glIndexPointer_fake_func type=%d, stride=%d,
            // byte_size=%d\n", type, stride, bytes_size);
            glIndexPointer(type, stride,
                           process->current_state->indexPointer);
            break;
        }

    case glEdgeFlagPointer_fake_func:
        {
            int offset = args[0];
            int stride = args[1];
            int bytes_size = args[2];

            process->current_state->edgeFlagPointerSize =
                MAX(process->current_state->edgeFlagPointerSize,
                    offset + bytes_size);
            process->current_state->edgeFlagPointer =
                qemu_realloc(process->current_state->edgeFlagPointer,
                        process->current_state->edgeFlagPointerSize);
            memcpy(process->current_state->edgeFlagPointer + offset,
                   (void *) args[3], bytes_size);
            // DEBUGF( "glEdgeFlagPointer_fake_func stride = %d,
            // bytes_size=%d\n", stride, bytes_size);
            glEdgeFlagPointer(stride,
                              process->current_state->edgeFlagPointer);
            break;
        }

    case glVertexAttribPointerARB_fake_func:
        {
            GET_EXT_PTR(void, glVertexAttribPointerARB,
                        (int, int, int, int, int, void *));
            int offset = args[0];
            int index = args[1];
            int size = args[2];
            int type = args[3];
            int normalized = args[4];
            int stride = args[5];
            int bytes_size = args[6];

            process->current_state->vertexAttribPointerSize[index] =
                MAX(process->current_state->vertexAttribPointerSize[index],
                    offset + bytes_size);
            process->current_state->vertexAttribPointer[index] =
                qemu_realloc(process->current_state->vertexAttribPointer[index],
                        process->current_state->
                        vertexAttribPointerSize[index]);
            memcpy(process->current_state->vertexAttribPointer[index] +
                   offset, (void *) args[7], bytes_size);
            ptr_func_glVertexAttribPointerARB(index, size, type, normalized,
                                              stride,
                                              process->current_state->
                                              vertexAttribPointer[index]);
            break;
        }

    case glVertexAttribPointerNV_fake_func:
        {
            GET_EXT_PTR(void, glVertexAttribPointerNV,
                        (int, int, int, int, void *));
            int offset = args[0];
            int index = args[1];
            int size = args[2];
            int type = args[3];
            int stride = args[4];
            int bytes_size = args[5];

            process->current_state->vertexAttribPointerNVSize[index] =
                MAX(process->current_state->vertexAttribPointerNVSize[index],
                    offset + bytes_size);
            process->current_state->vertexAttribPointerNV[index] =
                qemu_realloc(process->current_state->vertexAttribPointerNV[index],
                        process->current_state->
                        vertexAttribPointerNVSize[index]);
            memcpy(process->current_state->vertexAttribPointerNV[index] +
                   offset, (void *) args[6], bytes_size);
            ptr_func_glVertexAttribPointerNV(index, size, type, stride,
                                             process->current_state->
                                             vertexAttribPointerNV[index]);
            break;
        }

    case glColorPointer_fake_func:
        {
            int offset = args[0];
            int size = args[1];
            int type = args[2];
            int stride = args[3];
            int bytes_size = args[4];

            process->current_state->colorPointerSize =
                MAX(process->current_state->colorPointerSize,
                    offset + bytes_size);
            process->current_state->colorPointer =
                qemu_realloc(process->current_state->colorPointer,
                        process->current_state->colorPointerSize);
            memcpy(process->current_state->colorPointer + offset,
                   (void *) args[5], bytes_size);
            // DEBUGF( "glColorPointer_fake_func bytes_size = %d\n",
            // bytes_size);
            glColorPointer(size, type, stride,
                           process->current_state->colorPointer);

            break;
        }

    case glSecondaryColorPointer_fake_func:
        {
            GET_EXT_PTR(void, glSecondaryColorPointer,
                        (int, int, int, void *));
            int offset = args[0];
            int size = args[1];
            int type = args[2];
            int stride = args[3];
            int bytes_size = args[4];

            process->current_state->secondaryColorPointerSize =
                MAX(process->current_state->secondaryColorPointerSize,
                    offset + bytes_size);
            process->current_state->secondaryColorPointer =
                qemu_realloc(process->current_state->secondaryColorPointer,
                        process->current_state->secondaryColorPointerSize);
            memcpy(process->current_state->secondaryColorPointer + offset,
                   (void *) args[5], bytes_size);
            // DEBUGF( "glSecondaryColorPointer_fake_func bytes_size
            // = %d\n", bytes_size);
            ptr_func_glSecondaryColorPointer(size, type, stride,
                                             process->current_state->
                                             secondaryColorPointer);

            break;
        }

    case glPushClientAttrib_func:
        {
            int mask = args[0];

            if (process->current_state->clientStateSp <
                MAX_CLIENT_STATE_STACK_SIZE) {
                process->current_state->clientStateStack[process->
                                                         current_state->
                                                         clientStateSp].mask =
                    mask;
                if (mask & GL_CLIENT_VERTEX_ARRAY_BIT) {
                    process->current_state->clientStateStack[process->
                                                             current_state->
                                                             clientStateSp].
                        activeTextureIndex =
                        process->current_state->activeTextureIndex;
                }
                process->current_state->clientStateSp++;
            }
            glPushClientAttrib(mask);
            break;
        }

    case glPopClientAttrib_func:
        {
            if (process->current_state->clientStateSp > 0) {
                process->current_state->clientStateSp--;
                if (process->current_state->
                    clientStateStack[process->current_state->clientStateSp].
                    mask & GL_CLIENT_VERTEX_ARRAY_BIT) {
                    process->current_state->activeTextureIndex =
                        process->current_state->clientStateStack[process->
                                                                 current_state->
                                                                 clientStateSp].
                        activeTextureIndex;
                }
            }
            glPopClientAttrib();
            break;
        }

    case glClientActiveTexture_func:
    case glClientActiveTextureARB_func:
        {
            int activeTexture = args[0];

            process->current_state->activeTextureIndex =
                activeTexture - GL_TEXTURE0_ARB;
            do_glClientActiveTextureARB(activeTexture);
            break;
        }

    case glTexCoordPointer_fake_func:
        {
            int offset = args[0];
            int index = args[1];
            int size = args[2];
            int type = args[3];
            int stride = args[4];
            int bytes_size = args[5];

            process->current_state->texCoordPointerSize[index] =
                MAX(process->current_state->texCoordPointerSize[index],
                    offset + bytes_size);
            process->current_state->texCoordPointer[index] =
                qemu_realloc(process->current_state->texCoordPointer[index],
                        process->current_state->texCoordPointerSize[index]);
            memcpy(process->current_state->texCoordPointer[index] + offset,
                   (void *) args[6], bytes_size);
            /* DEBUGF( "glTexCoordPointer_fake_func size=%d, type=%d, 
             * stride=%d, byte_size=%d\n", size, type, stride, bytes_size); */
            do_glClientActiveTextureARB(GL_TEXTURE0_ARB + index);
            glTexCoordPointer(size, type, stride,
                              process->current_state->texCoordPointer[index]);
            do_glClientActiveTextureARB(GL_TEXTURE0_ARB +
                                        process->current_state->
                                        activeTextureIndex);
            break;
        }

    case glWeightPointerARB_fake_func:
        {
            GET_EXT_PTR(void, glWeightPointerARB, (int, int, int, void *));
            int offset = args[0];
            int size = args[1];
            int type = args[2];
            int stride = args[3];
            int bytes_size = args[4];

            process->current_state->weightPointerSize =
                MAX(process->current_state->weightPointerSize,
                    offset + bytes_size);
            process->current_state->weightPointer =
                qemu_realloc(process->current_state->weightPointer,
                        process->current_state->weightPointerSize);
            memcpy(process->current_state->weightPointer + offset,
                   (void *) args[5], bytes_size);
            /* DEBUGF( "glWeightPointerARB_fake_func size=%d,
             * type=%d, stride=%d, byte_size=%d\n", size, type, stride,
             * bytes_size); */
            ptr_func_glWeightPointerARB(size, type, stride,
                                        process->current_state->
                                        weightPointer);
            break;
        }

    case glMatrixIndexPointerARB_fake_func:
        {
            GET_EXT_PTR(void, glMatrixIndexPointerARB,
                        (int, int, int, void *));
            int offset = args[0];
            int size = args[1];
            int type = args[2];
            int stride = args[3];
            int bytes_size = args[4];

            process->current_state->matrixIndexPointerSize =
                MAX(process->current_state->matrixIndexPointerSize,
                    offset + bytes_size);
            process->current_state->matrixIndexPointer =
                qemu_realloc(process->current_state->matrixIndexPointer,
                        process->current_state->matrixIndexPointerSize);
            memcpy(process->current_state->matrixIndexPointer + offset,
                   (void *) args[5], bytes_size);
            /* DEBUGF( "glMatrixIndexPointerARB_fake_func size=%d,
             * type=%d, stride=%d, byte_size=%d\n", size, type, stride,
             * bytes_size); */
            ptr_func_glMatrixIndexPointerARB(size, type, stride,
                                             process->current_state->
                                             matrixIndexPointer);
            break;
        }

    case glFogCoordPointer_fake_func:
        {
            GET_EXT_PTR(void, glFogCoordPointer, (int, int, void *));
            int offset = args[0];
            int type = args[1];
            int stride = args[2];
            int bytes_size = args[3];

            process->current_state->fogCoordPointerSize =
                MAX(process->current_state->fogCoordPointerSize,
                    offset + bytes_size);
            process->current_state->fogCoordPointer =
                qemu_realloc(process->current_state->fogCoordPointer,
                        process->current_state->fogCoordPointerSize);
            memcpy(process->current_state->fogCoordPointer + offset,
                   (void *) args[4], bytes_size);
            // DEBUGF( "glFogCoordPointer_fake_func type=%d,
            // stride=%d, byte_size=%d\n", type, stride, bytes_size);
            ptr_func_glFogCoordPointer(type, stride,
                                       process->current_state->
                                       fogCoordPointer);
            break;
        }

    case glVariantPointerEXT_fake_func:
        {
            GET_EXT_PTR(void, glVariantPointerEXT, (int, int, int, void *));
            int offset = args[0];
            int id = args[1];
            int type = args[2];
            int stride = args[3];
            int bytes_size = args[4];

            process->current_state->variantPointerEXTSize[id] =
                MAX(process->current_state->variantPointerEXTSize[id],
                    offset + bytes_size);
            process->current_state->variantPointerEXT[id] =
                qemu_realloc(process->current_state->variantPointerEXT[id],
                        process->current_state->variantPointerEXTSize[id]);
            memcpy(process->current_state->variantPointerEXT[id] + offset,
                   (void *) args[5], bytes_size);
            // DEBUGF( "glVariantPointerEXT_fake_func[%d] type=%d,
            // stride=%d, byte_size=%d\n", id, type, stride, bytes_size);
            ptr_func_glVariantPointerEXT(id, type, stride,
                                         process->current_state->
                                         variantPointerEXT[id]);
            break;
        }

    case glInterleavedArrays_fake_func:
        {
            GET_EXT_PTR(void, glInterleavedArrays, (int, int, void *));
            int offset = args[0];
            int format = args[1];
            int stride = args[2];
            int bytes_size = args[3];

            process->current_state->interleavedArraysSize =
                MAX(process->current_state->interleavedArraysSize,
                    offset + bytes_size);
            process->current_state->interleavedArrays =
                qemu_realloc(process->current_state->interleavedArrays,
                        process->current_state->interleavedArraysSize);
            memcpy(process->current_state->interleavedArrays + offset,
                   (void *) args[4], bytes_size);
            // DEBUGF( "glInterleavedArrays_fake_func format=%d,
            // stride=%d, byte_size=%d\n", format, stride, bytes_size);
            ptr_func_glInterleavedArrays(format, stride,
                                         process->current_state->
                                         interleavedArrays);
            break;
        }

    case glElementPointerATI_fake_func:
        {
            GET_EXT_PTR(void, glElementPointerATI, (int, void *));
            int type = args[0];
            int bytes_size = args[1];

            process->current_state->elementPointerATISize = bytes_size;
            process->current_state->elementPointerATI =
                qemu_realloc(process->current_state->elementPointerATI,
                        process->current_state->elementPointerATISize);
            memcpy(process->current_state->elementPointerATI,
                   (void *) args[2], bytes_size);
            // DEBUGF( "glElementPointerATI_fake_func type=%d,
            // byte_size=%d\n", type, bytes_size);
            ptr_func_glElementPointerATI(type,
                                         process->current_state->
                                         elementPointerATI);
            break;
        }

    case glTexCoordPointer01_fake_func:
        {
            int size = args[0];
            int type = args[1];
            int stride = args[2];
            int bytes_size = args[3];

            process->current_state->texCoordPointerSize[0] = bytes_size;
            process->current_state->texCoordPointer[0] =
                qemu_realloc(process->current_state->texCoordPointer[0],
                        bytes_size);
            memcpy(process->current_state->texCoordPointer[0],
                   (void *) args[4], bytes_size);
            /* DEBUGF( "glTexCoordPointer01_fake_func size=%d,
             * type=%d, stride=%d, byte_size=%d\n", size, type, stride,
             * bytes_size); */
            do_glClientActiveTextureARB(GL_TEXTURE0_ARB + 0);
            glTexCoordPointer(size, type, stride,
                              process->current_state->texCoordPointer[0]);
            do_glClientActiveTextureARB(GL_TEXTURE0_ARB + 1);
            glTexCoordPointer(size, type, stride,
                              process->current_state->texCoordPointer[0]);
            do_glClientActiveTextureARB(GL_TEXTURE0_ARB +
                                        process->current_state->
                                        activeTextureIndex);
            break;
        }

    case glTexCoordPointer012_fake_func:
        {
            int size = args[0];
            int type = args[1];
            int stride = args[2];
            int bytes_size = args[3];

            process->current_state->texCoordPointerSize[0] = bytes_size;
            process->current_state->texCoordPointer[0] =
                qemu_realloc(process->current_state->texCoordPointer[0],
                        bytes_size);
            memcpy(process->current_state->texCoordPointer[0],
                   (void *) args[4], bytes_size);
            /* DEBUGF( "glTexCoordPointer012_fake_func size=%d,
             * type=%d, stride=%d, byte_size=%d\n", size, type, stride,
             * bytes_size); */
            do_glClientActiveTextureARB(GL_TEXTURE0_ARB + 0);
            glTexCoordPointer(size, type, stride,
                              process->current_state->texCoordPointer[0]);
            do_glClientActiveTextureARB(GL_TEXTURE0_ARB + 1);
            glTexCoordPointer(size, type, stride,
                              process->current_state->texCoordPointer[0]);
            do_glClientActiveTextureARB(GL_TEXTURE0_ARB + 2);
            glTexCoordPointer(size, type, stride,
                              process->current_state->texCoordPointer[0]);
            do_glClientActiveTextureARB(GL_TEXTURE0_ARB +
                                        process->current_state->
                                        activeTextureIndex);
            break;
        }

    case glVertexAndNormalPointer_fake_func:
        {
            int vertexPointerSize = args[0];
            int vertexPointerType = args[1];
            int vertexPointerStride = args[2];
            int normalPointerType = args[3];
            int normalPointerStride = args[4];
            int bytes_size = args[5];
            void *ptr = (void *) args[6];

            process->current_state->vertexPointerSize = bytes_size;
            process->current_state->vertexPointer =
                qemu_realloc(process->current_state->vertexPointer, bytes_size);
            memcpy(process->current_state->vertexPointer, ptr, bytes_size);
            glVertexPointer(vertexPointerSize, vertexPointerType,
                            vertexPointerStride,
                            process->current_state->vertexPointer);
            glNormalPointer(normalPointerType, normalPointerStride,
                            process->current_state->vertexPointer);
            break;
        }

    case glVertexNormalPointerInterlaced_fake_func:
        {
            int i = 0;
            int offset = args[i++];
            int vertexPointerSize = args[i++];
            int vertexPointerType = args[i++];
            int stride = args[i++];
            int normalPointerOffset = args[i++];
            int normalPointerType = args[i++];
            int bytes_size = args[i++];
            void *ptr = (void *) args[i++];

            process->current_state->vertexPointerSize =
                MAX(process->current_state->vertexPointerSize,
                    offset + bytes_size);
            process->current_state->vertexPointer =
                qemu_realloc(process->current_state->vertexPointer,
                        process->current_state->vertexPointerSize);
            memcpy(process->current_state->vertexPointer + offset, ptr,
                   bytes_size);
            glVertexPointer(vertexPointerSize, vertexPointerType, stride,
                            process->current_state->vertexPointer);
            glNormalPointer(normalPointerType, stride,
                            process->current_state->vertexPointer +
                            normalPointerOffset);
            break;
        }

    case glTuxRacerDrawElements_fake_func:
        {
            int mode = args[0];
            int count = args[1];
            int isColorEnabled = args[2];
            void *ptr = (void *) args[3];
            int stride =
                6 * sizeof(float) +
                ((isColorEnabled) ? 4 * sizeof(unsigned char) : 0);
            glVertexPointer(3, GL_FLOAT, stride, ptr);
            glNormalPointer(GL_FLOAT, stride, ptr + 3 * sizeof(float));
            if (isColorEnabled)
                glColorPointer(4, GL_UNSIGNED_BYTE, stride,
                               ptr + 6 * sizeof(float));
            glDrawArrays(mode, 0, count);
            break;
        }

    case glVertexNormalColorPointerInterlaced_fake_func:
        {
            int i = 0;
            int offset = args[i++];
            int vertexPointerSize = args[i++];
            int vertexPointerType = args[i++];
            int stride = args[i++];
            int normalPointerOffset = args[i++];
            int normalPointerType = args[i++];
            int colorPointerOffset = args[i++];
            int colorPointerSize = args[i++];
            int colorPointerType = args[i++];
            int bytes_size = args[i++];
            void *ptr = (void *) args[i++];

            process->current_state->vertexPointerSize =
                MAX(process->current_state->vertexPointerSize,
                    offset + bytes_size);
            process->current_state->vertexPointer =
                qemu_realloc(process->current_state->vertexPointer,
                        process->current_state->vertexPointerSize);
            memcpy(process->current_state->vertexPointer + offset, ptr,
                   bytes_size);
            glVertexPointer(vertexPointerSize, vertexPointerType, stride,
                            process->current_state->vertexPointer);
            glNormalPointer(normalPointerType, stride,
                            process->current_state->vertexPointer +
                            normalPointerOffset);
            glColorPointer(colorPointerSize, colorPointerType, stride,
                           process->current_state->vertexPointer +
                           colorPointerOffset);
            break;
        }

    case glVertexColorTexCoord0PointerInterlaced_fake_func:
        {
            int i = 0;
            int offset = args[i++];
            int vertexPointerSize = args[i++];
            int vertexPointerType = args[i++];
            int stride = args[i++];
            int colorPointerOffset = args[i++];
            int colorPointerSize = args[i++];
            int colorPointerType = args[i++];
            int texCoord0PointerOffset = args[i++];
            int texCoord0PointerSize = args[i++];
            int texCoord0PointerType = args[i++];
            int bytes_size = args[i++];
            void *ptr = (void *) args[i++];

            process->current_state->vertexPointerSize =
                MAX(process->current_state->vertexPointerSize,
                    offset + bytes_size);
            process->current_state->vertexPointer =
                qemu_realloc(process->current_state->vertexPointer,
                        process->current_state->vertexPointerSize);
            memcpy(process->current_state->vertexPointer + offset, ptr,
                   bytes_size);
            glVertexPointer(vertexPointerSize, vertexPointerType, stride,
                            process->current_state->vertexPointer);
            glColorPointer(colorPointerSize, colorPointerType, stride,
                           process->current_state->vertexPointer +
                           colorPointerOffset);
            do_glClientActiveTextureARB(GL_TEXTURE0_ARB + 0);
            glTexCoordPointer(texCoord0PointerSize, texCoord0PointerType,
                              stride,
                              process->current_state->vertexPointer +
                              texCoord0PointerOffset);
            do_glClientActiveTextureARB(GL_TEXTURE0_ARB +
                                        process->current_state->
                                        activeTextureIndex);
            break;
        }

    case glVertexNormalTexCoord0PointerInterlaced_fake_func:
        {
            int i = 0;
            int offset = args[i++];
            int vertexPointerSize = args[i++];
            int vertexPointerType = args[i++];
            int stride = args[i++];
            int normalPointerOffset = args[i++];
            int normalPointerType = args[i++];
            int texCoord0PointerOffset = args[i++];
            int texCoord0PointerSize = args[i++];
            int texCoord0PointerType = args[i++];
            int bytes_size = args[i++];
            void *ptr = (void *) args[i++];

            process->current_state->vertexPointerSize =
                MAX(process->current_state->vertexPointerSize,
                    offset + bytes_size);
            process->current_state->vertexPointer =
                qemu_realloc(process->current_state->vertexPointer,
                        process->current_state->vertexPointerSize);
            memcpy(process->current_state->vertexPointer + offset, ptr,
                   bytes_size);
            glVertexPointer(vertexPointerSize, vertexPointerType, stride,
                            process->current_state->vertexPointer);
            glNormalPointer(normalPointerType, stride,
                            process->current_state->vertexPointer +
                            normalPointerOffset);
            do_glClientActiveTextureARB(GL_TEXTURE0_ARB + 0);
            glTexCoordPointer(texCoord0PointerSize, texCoord0PointerType,
                              stride,
                              process->current_state->vertexPointer +
                              texCoord0PointerOffset);
            do_glClientActiveTextureARB(GL_TEXTURE0_ARB +
                                        process->current_state->
                                        activeTextureIndex);
            break;
        }

    case glVertexNormalTexCoord01PointerInterlaced_fake_func:
        {
            int i = 0;
            int offset = args[i++];
            int vertexPointerSize = args[i++];
            int vertexPointerType = args[i++];
            int stride = args[i++];
            int normalPointerOffset = args[i++];
            int normalPointerType = args[i++];
            int texCoord0PointerOffset = args[i++];
            int texCoord0PointerSize = args[i++];
            int texCoord0PointerType = args[i++];
            int texCoord1PointerOffset = args[i++];
            int texCoord1PointerSize = args[i++];
            int texCoord1PointerType = args[i++];
            int bytes_size = args[i++];
            void *ptr = (void *) args[i++];

            process->current_state->vertexPointerSize =
                MAX(process->current_state->vertexPointerSize,
                    offset + bytes_size);
            process->current_state->vertexPointer =
                qemu_realloc(process->current_state->vertexPointer,
                        process->current_state->vertexPointerSize);
            memcpy(process->current_state->vertexPointer + offset, ptr,
                   bytes_size);
            glVertexPointer(vertexPointerSize, vertexPointerType, stride,
                            process->current_state->vertexPointer);
            glNormalPointer(normalPointerType, stride,
                            process->current_state->vertexPointer +
                            normalPointerOffset);
            do_glClientActiveTextureARB(GL_TEXTURE0_ARB + 0);
            glTexCoordPointer(texCoord0PointerSize, texCoord0PointerType,
                              stride,
                              process->current_state->vertexPointer +
                              texCoord0PointerOffset);
            do_glClientActiveTextureARB(GL_TEXTURE0_ARB + 1);
            glTexCoordPointer(texCoord1PointerSize, texCoord1PointerType,
                              stride,
                              process->current_state->vertexPointer +
                              texCoord1PointerOffset);
            do_glClientActiveTextureARB(GL_TEXTURE0_ARB +
                                        process->current_state->
                                        activeTextureIndex);
            break;
        }

    case glVertexNormalTexCoord012PointerInterlaced_fake_func:
        {
            int i = 0;
            int offset = args[i++];
            int vertexPointerSize = args[i++];
            int vertexPointerType = args[i++];
            int stride = args[i++];
            int normalPointerOffset = args[i++];
            int normalPointerType = args[i++];
            int texCoord0PointerOffset = args[i++];
            int texCoord0PointerSize = args[i++];
            int texCoord0PointerType = args[i++];
            int texCoord1PointerOffset = args[i++];
            int texCoord1PointerSize = args[i++];
            int texCoord1PointerType = args[i++];
            int texCoord2PointerOffset = args[i++];
            int texCoord2PointerSize = args[i++];
            int texCoord2PointerType = args[i++];
            int bytes_size = args[i++];
            void *ptr = (void *) args[i++];

            process->current_state->vertexPointerSize =
                MAX(process->current_state->vertexPointerSize,
                    offset + bytes_size);
            process->current_state->vertexPointer =
                qemu_realloc(process->current_state->vertexPointer,
                        process->current_state->vertexPointerSize);
            memcpy(process->current_state->vertexPointer + offset, ptr,
                   bytes_size);
            glVertexPointer(vertexPointerSize, vertexPointerType, stride,
                            process->current_state->vertexPointer);
            glNormalPointer(normalPointerType, stride,
                            process->current_state->vertexPointer +
                            normalPointerOffset);
            do_glClientActiveTextureARB(GL_TEXTURE0_ARB + 0);
            glTexCoordPointer(texCoord0PointerSize, texCoord0PointerType,
                              stride,
                              process->current_state->vertexPointer +
                              texCoord0PointerOffset);
            do_glClientActiveTextureARB(GL_TEXTURE0_ARB + 1);
            glTexCoordPointer(texCoord1PointerSize, texCoord1PointerType,
                              stride,
                              process->current_state->vertexPointer +
                              texCoord1PointerOffset);
            do_glClientActiveTextureARB(GL_TEXTURE0_ARB + 2);
            glTexCoordPointer(texCoord2PointerSize, texCoord2PointerType,
                              stride,
                              process->current_state->vertexPointer +
                              texCoord2PointerOffset);
            do_glClientActiveTextureARB(GL_TEXTURE0_ARB +
                                        process->current_state->
                                        activeTextureIndex);
            break;
        }

    case glVertexNormalColorTexCoord0PointerInterlaced_fake_func:
        {
            int i = 0;
            int offset = args[i++];
            int vertexPointerSize = args[i++];
            int vertexPointerType = args[i++];
            int stride = args[i++];
            int normalPointerOffset = args[i++];
            int normalPointerType = args[i++];
            int colorPointerOffset = args[i++];
            int colorPointerSize = args[i++];
            int colorPointerType = args[i++];
            int texCoord0PointerOffset = args[i++];
            int texCoord0PointerSize = args[i++];
            int texCoord0PointerType = args[i++];
            int bytes_size = args[i++];
            void *ptr = (void *) args[i++];

            process->current_state->vertexPointerSize =
                MAX(process->current_state->vertexPointerSize,
                    offset + bytes_size);
            process->current_state->vertexPointer =
                qemu_realloc(process->current_state->vertexPointer,
                        process->current_state->vertexPointerSize);
            memcpy(process->current_state->vertexPointer + offset, ptr,
                   bytes_size);
            glVertexPointer(vertexPointerSize, vertexPointerType, stride,
                            process->current_state->vertexPointer);
            glNormalPointer(normalPointerType, stride,
                            process->current_state->vertexPointer +
                            normalPointerOffset);
            glColorPointer(colorPointerSize, colorPointerType, stride,
                           process->current_state->vertexPointer +
                           colorPointerOffset);
            do_glClientActiveTextureARB(GL_TEXTURE0_ARB + 0);
            glTexCoordPointer(texCoord0PointerSize, texCoord0PointerType,
                              stride,
                              process->current_state->vertexPointer +
                              texCoord0PointerOffset);
            do_glClientActiveTextureARB(GL_TEXTURE0_ARB +
                                        process->current_state->
                                        activeTextureIndex);
            break;
        }

    case glVertexNormalColorTexCoord01PointerInterlaced_fake_func:
        {
            int i = 0;
            int offset = args[i++];
            int vertexPointerSize = args[i++];
            int vertexPointerType = args[i++];
            int stride = args[i++];
            int normalPointerOffset = args[i++];
            int normalPointerType = args[i++];
            int colorPointerOffset = args[i++];
            int colorPointerSize = args[i++];
            int colorPointerType = args[i++];
            int texCoord0PointerOffset = args[i++];
            int texCoord0PointerSize = args[i++];
            int texCoord0PointerType = args[i++];
            int texCoord1PointerOffset = args[i++];
            int texCoord1PointerSize = args[i++];
            int texCoord1PointerType = args[i++];
            int bytes_size = args[i++];
            void *ptr = (void *) args[i++];

            process->current_state->vertexPointerSize =
                MAX(process->current_state->vertexPointerSize,
                    offset + bytes_size);
            process->current_state->vertexPointer =
                qemu_realloc(process->current_state->vertexPointer,
                        process->current_state->vertexPointerSize);
            memcpy(process->current_state->vertexPointer + offset, ptr,
                   bytes_size);
            glVertexPointer(vertexPointerSize, vertexPointerType, stride,
                            process->current_state->vertexPointer);
            glNormalPointer(normalPointerType, stride,
                            process->current_state->vertexPointer +
                            normalPointerOffset);
            glColorPointer(colorPointerSize, colorPointerType, stride,
                           process->current_state->vertexPointer +
                           colorPointerOffset);
            do_glClientActiveTextureARB(GL_TEXTURE0_ARB + 0);
            glTexCoordPointer(texCoord0PointerSize, texCoord0PointerType,
                              stride,
                              process->current_state->vertexPointer +
                              texCoord0PointerOffset);
            do_glClientActiveTextureARB(GL_TEXTURE0_ARB + 1);
            glTexCoordPointer(texCoord1PointerSize, texCoord1PointerType,
                              stride,
                              process->current_state->vertexPointer +
                              texCoord1PointerOffset);
            do_glClientActiveTextureARB(GL_TEXTURE0_ARB +
                                        process->current_state->
                                        activeTextureIndex);
            break;
        }

    case glVertexNormalColorTexCoord012PointerInterlaced_fake_func:
        {
            int i = 0;
            int offset = args[i++];
            int vertexPointerSize = args[i++];
            int vertexPointerType = args[i++];
            int stride = args[i++];
            int normalPointerOffset = args[i++];
            int normalPointerType = args[i++];
            int colorPointerOffset = args[i++];
            int colorPointerSize = args[i++];
            int colorPointerType = args[i++];
            int texCoord0PointerOffset = args[i++];
            int texCoord0PointerSize = args[i++];
            int texCoord0PointerType = args[i++];
            int texCoord1PointerOffset = args[i++];
            int texCoord1PointerSize = args[i++];
            int texCoord1PointerType = args[i++];
            int texCoord2PointerOffset = args[i++];
            int texCoord2PointerSize = args[i++];
            int texCoord2PointerType = args[i++];
            int bytes_size = args[i++];
            void *ptr = (void *) args[i++];

            process->current_state->vertexPointerSize =
                MAX(process->current_state->vertexPointerSize,
                    offset + bytes_size);
            process->current_state->vertexPointer =
                qemu_realloc(process->current_state->vertexPointer,
                        process->current_state->vertexPointerSize);
            memcpy(process->current_state->vertexPointer + offset, ptr,
                   bytes_size);
            glVertexPointer(vertexPointerSize, vertexPointerType, stride,
                            process->current_state->vertexPointer);
            glNormalPointer(normalPointerType, stride,
                            process->current_state->vertexPointer +
                            normalPointerOffset);
            glColorPointer(colorPointerSize, colorPointerType, stride,
                           process->current_state->vertexPointer +
                           colorPointerOffset);
            do_glClientActiveTextureARB(GL_TEXTURE0_ARB + 0);
            glTexCoordPointer(texCoord0PointerSize, texCoord0PointerType,
                              stride,
                              process->current_state->vertexPointer +
                              texCoord0PointerOffset);
            do_glClientActiveTextureARB(GL_TEXTURE0_ARB + 1);
            glTexCoordPointer(texCoord1PointerSize, texCoord1PointerType,
                              stride,
                              process->current_state->vertexPointer +
                              texCoord1PointerOffset);
            do_glClientActiveTextureARB(GL_TEXTURE0_ARB + 2);
            glTexCoordPointer(texCoord2PointerSize, texCoord2PointerType,
                              stride,
                              process->current_state->vertexPointer +
                              texCoord2PointerOffset);
            do_glClientActiveTextureARB(GL_TEXTURE0_ARB +
                                        process->current_state->
                                        activeTextureIndex);
            break;
        }

    case _glVertexPointer_buffer_func:
        {
            glVertexPointer(args[0], args[1], args[2], (void *) args[3]);
            break;
        }

    case _glNormalPointer_buffer_func:
        {
            glNormalPointer(args[0], args[1], (void *) args[2]);
            break;
        }

    case _glColorPointer_buffer_func:
        {
            glColorPointer(args[0], args[1], args[2], (void *) args[3]);
            break;
        }

    case _glSecondaryColorPointer_buffer_func:
        {
            GET_EXT_PTR(void, glSecondaryColorPointer,
                        (int, int, int, void *));
            ptr_func_glSecondaryColorPointer(args[0], args[1], args[2],
                                             (void *) args[3]);
            break;
        }

    case _glIndexPointer_buffer_func:
        {
            glIndexPointer(args[0], args[1], (void *) args[2]);
            break;
        }

    case _glTexCoordPointer_buffer_func:
        {
            glTexCoordPointer(args[0], args[1], args[2], (void *) args[3]);
            break;
        }

    case _glEdgeFlagPointer_buffer_func:
        {
            glEdgeFlagPointer(args[0], (void *) args[1]);
            break;
        }

    case _glVertexAttribPointerARB_buffer_func:
        {
            GET_EXT_PTR(void, glVertexAttribPointerARB,
                        (int, int, int, int, int, void *));
            ptr_func_glVertexAttribPointerARB(args[0], args[1], args[2],
                                              args[3], args[4],
                                              (void *) args[5]);
            break;
        }

    case _glWeightPointerARB_buffer_func:
        {
            GET_EXT_PTR(void, glWeightPointerARB, (int, int, int, void *));

            ptr_func_glWeightPointerARB(args[0], args[1], args[2],
                                        (void *) args[3]);
            break;
        }

    case _glMatrixIndexPointerARB_buffer_func:
        {
            GET_EXT_PTR(void, glMatrixIndexPointerARB,
                        (int, int, int, void *));
            ptr_func_glMatrixIndexPointerARB(args[0], args[1], args[2],
                                             (void *) args[3]);
            break;
        }

    case _glFogCoordPointer_buffer_func:
        {
            GET_EXT_PTR(void, glFogCoordPointer, (int, int, void *));

            ptr_func_glFogCoordPointer(args[0], args[1], (void *) args[2]);
            break;
        }

    case _glVariantPointerEXT_buffer_func:
        {
            GET_EXT_PTR(void, glVariantPointerEXT, (int, int, int, void *));

            ptr_func_glVariantPointerEXT(args[0], args[1], args[2],
                                         (void *) args[3]);
            break;
        }

    case _glDrawElements_buffer_func:
        {
            glDrawElements(args[0], args[1], args[2], (void *) args[3]);
            break;
        }
#ifndef _WIN32
    //FIXME
    case _glDrawRangeElements_buffer_func:
        {
            glDrawRangeElements(args[0], args[1], args[2], args[3], args[4],
                                (void *) args[5]);
            break;
        }
#endif
    case _glMultiDrawElements_buffer_func:
        {
            GET_EXT_PTR(void, glMultiDrawElements,
                        (int, int *, int, void **, int));
            ptr_func_glMultiDrawElements(args[0], (int *) args[1], args[2],
                                         (void **) args[3], args[4]);
            break;
        }

    case _glGetError_fake_func:
        {
            break;
        }

    case glGetIntegerv_func:
        {
            glGetIntegerv(args[0], (int *) args[1]);
            break;
        }

    case _glReadPixels_pbo_func:
        {
            glReadPixels(ARG_TO_INT(args[0]), ARG_TO_INT(args[1]),
                         ARG_TO_INT(args[2]), ARG_TO_INT(args[3]),
                         ARG_TO_UNSIGNED_INT(args[4]),
                         ARG_TO_UNSIGNED_INT(args[5]), (void *) (args[6]));
            break;
        }

    case _glDrawPixels_pbo_func:
        {
            glDrawPixels(ARG_TO_INT(args[0]), ARG_TO_INT(args[1]),
                         ARG_TO_UNSIGNED_INT(args[2]),
                         ARG_TO_UNSIGNED_INT(args[3]),
                         (const void *) (args[4]));
            break;
        }

    case _glMapBufferARB_fake_func:
        {
            GET_EXT_PTR(GLvoid *, glMapBufferARB, (GLenum, GLenum));
            GET_EXT_PTR(GLboolean, glUnmapBufferARB, (GLenum));
            int target = args[0];
            int size = args[1];
            void *dst_ptr = (void *) args[2];
            void *src_ptr = ptr_func_glMapBufferARB(target, GL_READ_ONLY);

            if (src_ptr) {
                memcpy(dst_ptr, src_ptr, size);
                ret.i = ptr_func_glUnmapBufferARB(target);
            } else {
                ret.i = 0;
            }
            break;
        }

    case fake_gluBuild2DMipmaps_func:
        {
            GET_GLU_PTR(GLint, gluBuild2DMipmaps,
                        (GLenum arg_0, GLint arg_1, GLsizei arg_2,
                         GLsizei arg_3, GLenum arg_4, GLenum arg_5,
                         const GLvoid *arg_6));
            if (ptr_func_gluBuild2DMipmaps == NULL)
                ptr_func_gluBuild2DMipmaps = mesa_gluBuild2DMipmaps;
            ptr_func_gluBuild2DMipmaps(ARG_TO_UNSIGNED_INT(args[0]),
                                       ARG_TO_INT(args[1]),
                                       ARG_TO_INT(args[2]),
                                       ARG_TO_INT(args[3]),
                                       ARG_TO_UNSIGNED_INT(args[4]),
                                       ARG_TO_UNSIGNED_INT(args[5]),
                                       (const void *) (args[6]));
            break;
        }

    case _glSelectBuffer_fake_func:
        {
            process->current_state->selectBufferSize = args[0] * 4;
            process->current_state->selectBufferPtr =
                qemu_realloc(process->current_state->selectBufferPtr,
                        process->current_state->selectBufferSize);
            glSelectBuffer(args[0], process->current_state->selectBufferPtr);
            break;
        }

    case _glGetSelectBuffer_fake_func:
        {
            void *ptr = (void *) args[0];

            memcpy(ptr, process->current_state->selectBufferPtr,
                   process->current_state->selectBufferSize);
            break;
        }

    case _glFeedbackBuffer_fake_func:
        {
            process->current_state->feedbackBufferSize = args[0] * 4;
            process->current_state->feedbackBufferPtr =
                qemu_realloc(process->current_state->feedbackBufferPtr,
                        process->current_state->feedbackBufferSize);
            glFeedbackBuffer(args[0], args[1],
                             process->current_state->feedbackBufferPtr);
            break;
        }

    case _glGetFeedbackBuffer_fake_func:
        {
            void *ptr = (void *) args[0];

            memcpy(ptr, process->current_state->feedbackBufferPtr,
                   process->current_state->feedbackBufferSize);
            break;
        }

        /* 
         * case glEnableClientState_func: { if (display_function_call)
         * DEBUGF( "cap : %s\n", nameArrays[args[0] -
         * GL_VERTEX_ARRAY]); glEnableClientState(args[0]); break; }
         * 
         * case glDisableClientState_func: { if (display_function_call)
         * DEBUGF( "cap : %s\n", nameArrays[args[0] -
         * GL_VERTEX_ARRAY]); glDisableClientState(args[0]); break; }
         * 
         * case glClientActiveTexture_func: case
         * glClientActiveTextureARB_func: { if (display_function_call)
         * DEBUGF( "client activeTexture %d\n", args[0] -
         * GL_TEXTURE0_ARB); glClientActiveTextureARB(args[0]); break; }
         * 
         * case glActiveTextureARB_func: { if (display_function_call)
         * DEBUGF( "server activeTexture %d\n", args[0] -
         * GL_TEXTURE0_ARB); glActiveTextureARB(args[0]); break; }
         * 
         * case glLockArraysEXT_func: break;
         * 
         * case glUnlockArraysEXT_func: break;
         * 
         * case glArrayElement_func: { glArrayElement(args[0]); break; }
         * 
         * case glDrawArrays_func: { glDrawArrays(args[0],args[1],args[2]);
         * break; }
         * 
         * case glDrawElements_func: {
         * glDrawElements(args[0],args[1],args[2],(void*)args[3]); break; }
         * 
         * case glDrawRangeElements_func: {
         * glDrawRangeElements(args[0],args[1],args[2],args[3],args[4],(void*)args[5]);
         * break; } */

    case glGetError_func:
        {
            ret.i = glGetError();
            break;
        }

    case glNewObjectBufferATI_func:
        {
            GET_EXT_PTR(int, glNewObjectBufferATI, (int, void *, int));

            ret.i = ptr_func_glNewObjectBufferATI(args[0],
                            (void *) args[1], args[2]);
            break;
        }

    case glClear_func:
        /* HACK workaround for an unexplainable issue */
        if (args[0] & GL_COLOR_BUFFER_BIT)
            glClear(GL_COLOR_BUFFER_BIT);
        if (args[0] & GL_STENCIL_BUFFER_BIT)
            glClear(GL_STENCIL_BUFFER_BIT);
        if (args[0] & GL_DEPTH_BUFFER_BIT)
            glClear(GL_DEPTH_BUFFER_BIT);
        if (args[0] & GL_ACCUM_BUFFER_BIT)
            glClear(GL_ACCUM_BUFFER_BIT);
        break;

    default:
        execute_func(func_number, args, &ret);
        break;
    }

    switch (ret_type) {
    case TYPE_NONE:
    case TYPE_CHAR:
    case TYPE_UNSIGNED_CHAR:
    case TYPE_INT:
    case TYPE_UNSIGNED_INT:
        break;

    case TYPE_CONST_CHAR:
        {
            strncpy(ret_string, (ret.s) ? ret.s : "", 32768);
            break;
        }

    default:
        DEBUGF( "unexpected ret type : %d\n", ret_type);
        exit(-1);
        break;
    }

    if (display_function_call)
        DEBUGF( "[%d]< %s\n", process->p.process_id,
                tab_opengl_calls_name[func_number]);

    return ret.i;
}
