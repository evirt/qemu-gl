/*
 *  Host-side implementation of GL/GLX API
 *
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


#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>

#define GL_GLEXT_PROTOTYPES
#define GLX_GLXEXT_PROTOTYPES

#include <mesa_gl.h>
#include <mesa_glx.h>

// FIXME - probably breaks badly on 32 bit host w/ 64 bit guest
typedef long unsigned int target_phys_addr_t;

#include "opengl_func.h"

#include "mesa_glu.h"
#include "mesa_mipmap.c"

#include "../qemu-common.h"

//#define SYSTEMATIC_ERROR_CHECK
//#define BUFFER_BEGINEND

#define glGetError() 0

#define GET_EXT_PTR(type, funcname, args_decl) \
      static int detect_##funcname = 0; \
      static type(*ptr_func_##funcname)args_decl = NULL; \
      if (detect_##funcname == 0) \
      { \
        detect_##funcname = 1; \
        ptr_func_##funcname = (type(*)args_decl)glXGetProcAddressARB((const GLubyte*)#funcname); \
        assert (ptr_func_##funcname); \
      }

#define GET_EXT_PTR_NO_FAIL(type, funcname, args_decl) \
      static int detect_##funcname = 0; \
      static type(*ptr_func_##funcname)args_decl = NULL; \
      if (detect_##funcname == 0) \
      { \
        detect_##funcname = 1; \
        ptr_func_##funcname = (type(*)args_decl)glXGetProcAddressARB((const GLubyte*)#funcname); \
      }

#ifndef WIN32
#include <dlfcn.h>
#endif

static void *get_glu_ptr(const char *name)
{
    static void *handle = (void *) -1;

    if (handle == (void *) -1) {
#ifndef WIN32
        handle = dlopen("libGLU.so", RTLD_LAZY);
        if (!handle)
            fprintf(stderr, "can't load libGLU.so : %s\n", dlerror());
#else
        handle = (void *) LoadLibrary("glu32.dll");
        if (!handle)
            fprintf(stderr, "can't load glu32.dll\n");
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

static const int defaultAttribList[] = {
    GLX_RGBA,
    GLX_RED_SIZE, 1,
    GLX_GREEN_SIZE, 1,
    GLX_BLUE_SIZE, 1,
    GLX_DOUBLEBUFFER,
    None
};

static XVisualInfo *get_default_visual(Display *dpy)
{
    static XVisualInfo *vis = NULL;
    XVisualInfo theTemplate;
    int numVisuals;

    if (vis)
        return vis;
    fprintf(stderr, "get_default_visual\n");
    /* if (vis == NULL) vis = glXChooseVisual(dpy, 0,
     * (int*)defaultAttribList); */
    theTemplate.screen = 0;
    vis = XGetVisualInfo(dpy, VisualScreenMask, &theTemplate, &numVisuals);

    return vis;
}


static Display *parent_dpy = NULL;
static Window qemu_parent_window = 0;

static Window active_win = 0;   /* FIXME */
static int active_win_x = 0;
static int active_win_y = 0;

void opengl_exec_set_parent_window(Display *_dpy, Window _parent_window)
{
    parent_dpy = _dpy;
    qemu_parent_window = _parent_window;
    if (active_win)
        XReparentWindow(_dpy, active_win, _parent_window, active_win_x,
                        active_win_y);
}

static GLXDrawable create_window(Display *dpy, XVisualInfo *vis)
{
    int x=0, y=0, width=16, height=16;
    int scrnum;
    XSetWindowAttributes attr = { 0 };
    unsigned long mask;
    Window win;

    scrnum = DefaultScreen(dpy);

    /* window attributes */
    attr.background_pixel = 0xff000000;
    attr.border_pixel = 0;
    attr.colormap = XCreateColormap(dpy, qemu_parent_window, vis->visual, AllocNone);
    attr.event_mask = 0; /* StructureNotifyMask | ExposureMask | KeyPressMask */
    attr.save_under = True;
    attr.override_redirect = True;
    attr.cursor = None;
    mask =
        CWBackPixel | CWBorderPixel | CWColormap | CWEventMask |
        CWOverrideRedirect | CWSaveUnder;

    if (qemu_parent_window)
        win = XCreateWindow(dpy, qemu_parent_window, 0, 0, width, height, 0,
                        vis->depth, InputOutput, vis->visual, mask, &attr);

    /* set hints and properties */
    {
        XSizeHints sizehints;

        sizehints.x = x;
        sizehints.y = y;
        sizehints.width = width;
        sizehints.height = height;
        sizehints.flags = USSize | USPosition;
        XSetWMNormalHints(dpy, win, &sizehints);
        XSetStandardProperties(dpy, win, "", "", None,
                        (char **) NULL, 0, &sizehints);
    }

    XSync(dpy, 0);

    /* 
     * int loop = 1; while (loop) { while (XPending(dpy) > 0) { XEvent event;
     * XNextEvent(dpy, &event); switch (event.type) { case CreateNotify: { if
     * (((XCreateWindowEvent*)&event)->window == win) { loop = 0; } break; } }
     * } } */

    /* TODO */
    if (!active_win)
        active_win = win;

    return win;
}

typedef struct {
    void *key;
    void *value;
} Assoc;

#define MAX_HANDLED_PROCESS 100
#define MAX_ASSOC_SIZE 100

#define MAX_FBCONFIG 10

#include "opengl_utils.h"

#define MAX(a, b) (((a) > (b)) ? (a) : (b))


typedef struct {
    int x;
    int y;
    int width;
    int height;
    int map_state;
} WindowPosStruct;

typedef struct {
    GLbitfield mask;
    int activeTextureIndex;
} ClientState;

#define MAX_CLIENT_STATE_STACK_SIZE 16

typedef struct {
    int ref;
    int fake_ctxt;
    int fake_shareList;
    GLXContext context;
    GLXDrawable drawable;

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

#ifdef SYSTEMATIC_ERROR_CHECK
    int last_error;
#endif
} GLState;

typedef struct {
    int process_id;
    int instr_counter;

    int x, y, width, height;
    WindowPosStruct currentDrawablePos;

    int next_available_context_number;
    int next_available_pbuffer_number;

    int nb_states;
    GLState default_state;
    GLState **glstates;
    GLState *current_state;

    int nfbconfig;
    GLXFBConfig *fbconfigs[MAX_FBCONFIG];
    int fbconfigs_max[MAX_FBCONFIG];
    int nfbconfig_total;

    Assoc association_fakecontext_glxcontext[MAX_ASSOC_SIZE];
    Assoc association_fakepbuffer_pbuffer[MAX_ASSOC_SIZE];
    Assoc association_clientdrawable_serverdrawable[MAX_ASSOC_SIZE];
    Assoc association_fakecontext_visual[MAX_ASSOC_SIZE];

    Display *dpy;

    int began;
    int primitive;
    int bufsize;
    int bufstart;
    arg_t *cmdbuf;
} ProcessStruct;

static ProcessStruct processes[MAX_HANDLED_PROCESS];

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

/* ---- */

#ifdef BUFFER_BEGINEND
/* A user of the following two functions must not buffer any calls that
 * may throw an error (i.e. errors conditions must be checked before
 * storing in the buffer) or return values.  */
static inline arg_t *cmd_buffer_alloc(ProcessStruct *process, size_t elems)
{
    arg_t *ret;

    if (unlikely(process->bufstart + elems > process->bufsize)) {
        process->bufsize = (process->bufsize ?: 0x100) << 1;
        process->cmdbuf = qemu_realloc(process->cmdbuf,
                        process->bufsize * sizeof(arg_t));
    }

    ret = process->cmdbuf + process->bufstart;
    process->bufstart += elems;
    return ret;
}

static inline void cmd_buffer_replay(ProcessStruct *process)
{
    Signature *sig;
    int func_number;
    union gl_ret_type ret;
    arg_t *call = process->cmdbuf;

    while (process->bufstart) {
        func_number = *call ++;
        sig = (Signature *) tab_opengl_calls[func_number];

        execute_func(func_number, call, &ret);

        call += sig->nb_args;
        process->bufstart -= sig->nb_args + 1;
    }
}
#endif

/* ---- */

typedef void *ClientGLXDrawable;
static inline ClientGLXDrawable to_drawable(arg_t arg)
{
#ifdef TARGET_X86_64
    if (arg > (unsigned long) -1) {
        fprintf(stderr, "GLXDrawable too big for this implementation\n");
        exit(-1);
    }
#endif
    return (void *) (unsigned long) arg;
}

GLXContext get_association_fakecontext_glxcontext(
                ProcessStruct *process, int fakecontext)
{
    int i;

    for (i = 0;
         i < MAX_ASSOC_SIZE &&
         process->association_fakecontext_glxcontext[i].key; i++)
        if ((int) (long) process->association_fakecontext_glxcontext[i].key ==
                        fakecontext)
            return (GLXContext)
                    process->association_fakecontext_glxcontext[i].value;

    return NULL;
}

void set_association_fakecontext_glxcontext(
                ProcessStruct *process, int fakecontext, GLXContext glxcontext)
{
    int i;

    for (i = 0;
         i < MAX_ASSOC_SIZE &&
         process->association_fakecontext_glxcontext[i].key; i++)
        if ((int) (long) process->association_fakecontext_glxcontext[i].key ==
                        fakecontext)
            break;

    if (i < MAX_ASSOC_SIZE) {
        process->association_fakecontext_glxcontext[i].key =
                (void *) (long) fakecontext;
        process->association_fakecontext_glxcontext[i].value =
                (void *) glxcontext;
    } else
        fprintf(stderr, "MAX_ASSOC_SIZE reached\n");
}

void unset_association_fakecontext_glxcontext(
                ProcessStruct *process, int fakecontext)
{
    int i;

    for (i = 0;
         i < MAX_ASSOC_SIZE &&
         process->association_fakecontext_glxcontext[i].key; i++)
        if ((int) (long) process->association_fakecontext_glxcontext[i].key ==
                        fakecontext) {
            memmove(&process->association_fakecontext_glxcontext[i],
                    &process->association_fakecontext_glxcontext[i + 1],
                    sizeof(Assoc) * (MAX_ASSOC_SIZE - 1 - i));
            return;
        }
}

/* ---- */

XVisualInfo *get_association_fakecontext_visual(
                ProcessStruct *process, int fakecontext)
{
    int i;

    for (i = 0;
         i < MAX_ASSOC_SIZE && process->association_fakecontext_visual[i].key;
         i++)
        if ((int) (long) process->association_fakecontext_visual[i].key ==
                        fakecontext)
            return process->association_fakecontext_visual[i].value;

    return NULL;
}

void set_association_fakecontext_visual(ProcessStruct *process,
                int fakecontext, XVisualInfo *visual)
{
    int i;

    for (i = 0;
         i < MAX_ASSOC_SIZE && process->association_fakecontext_visual[i].key;
         i++)
        if ((int) (long) process->association_fakecontext_visual[i].key ==
                        fakecontext)
            break;

    if (i < MAX_ASSOC_SIZE) {
        process->association_fakecontext_visual[i].key =
                (void *) (long) fakecontext;
        process->association_fakecontext_visual[i].value = (void *) visual;
    } else
        fprintf(stderr, "MAX_ASSOC_SIZE reached\n");
}

/* ---- */

GLXPbuffer get_association_fakepbuffer_pbuffer(
                ProcessStruct *process, ClientGLXDrawable fakepbuffer)
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

void set_association_fakepbuffer_pbuffer(ProcessStruct *process,
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
        fprintf(stderr, "MAX_ASSOC_SIZE reached\n");
}

void unset_association_fakepbuffer_pbuffer(ProcessStruct *process,
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

/* ---- */

GLXDrawable get_association_clientdrawable_serverdrawable(
                ProcessStruct *process, ClientGLXDrawable clientdrawable)
{
    int i;

    for (i = 0; i < MAX_ASSOC_SIZE &&
                    process->association_clientdrawable_serverdrawable[i].key;
                    i++)
        if ((ClientGLXDrawable) process->
                        association_clientdrawable_serverdrawable[i].key ==
                        clientdrawable)
            return (GLXDrawable) process->
                association_clientdrawable_serverdrawable[i].value;

    return (GLXDrawable) 0;
}

ClientGLXDrawable get_association_serverdrawable_clientdrawable(
                ProcessStruct *process, GLXDrawable serverdrawable)
{
    int i;

    for (i = 0; i < MAX_ASSOC_SIZE &&
                    process->association_clientdrawable_serverdrawable[i].key;
                    i ++)
        if ((GLXDrawable) process->
                        association_clientdrawable_serverdrawable[i].value ==
                        serverdrawable)
            return (ClientGLXDrawable)
                    process->association_clientdrawable_serverdrawable[i].key;

    return NULL;
}

void set_association_clientdrawable_serverdrawable(
                ProcessStruct *process, ClientGLXDrawable clientdrawable,
                GLXDrawable serverdrawable)
{
    int i;

    for (i = 0; process->association_clientdrawable_serverdrawable[i].key;
                    i ++)
        if ((ClientGLXDrawable) process->
                        association_clientdrawable_serverdrawable[i].key ==
                        clientdrawable)
            break;

    if (i < MAX_ASSOC_SIZE) {
        process->association_clientdrawable_serverdrawable[i].key =
                (void *) clientdrawable;
        process->association_clientdrawable_serverdrawable[i].value =
                (void *) serverdrawable;
    } else
        fprintf(stderr, "MAX_ASSOC_SIZE reached\n");
}

static void _get_window_pos(Display *dpy, Window win, WindowPosStruct *pos)
{
    XWindowAttributes window_attributes_return;
    Window child;
    int x, y;
    Window root = DefaultRootWindow(dpy);

    XGetWindowAttributes(dpy, win, &window_attributes_return);
    XTranslateCoordinates(dpy, win, root, 0, 0, &x, &y, &child);
    /* printf("%d %d %d %d\n", x, y, window_attributes_return.width,
     * window_attributes_return.height); */
    pos->x = x;
    pos->y = y;
    pos->width = window_attributes_return.width;
    pos->height = window_attributes_return.height;
    pos->map_state = window_attributes_return.map_state;
}

static int is_gl_vendor_ati(Display *dpy)
{
    static int is_gl_vendor_ati_flag = 0;
    static int has_init = 0;

    if (has_init == 0) {
        has_init = 1;
        is_gl_vendor_ati_flag =
            (strncmp(glXGetClientString(dpy, GLX_VENDOR), "ATI", 3) == 0);
    }
    return is_gl_vendor_ati_flag;
}

static int get_server_texture(ProcessStruct *process,
                              unsigned int client_texture)
{
    unsigned int server_texture = 0;

    if (client_texture < 32768) {
        server_texture = process->current_state->tabTextures[client_texture];
    } else {
        fprintf(stderr, "invalid texture name %d\n", client_texture);
    }
    return server_texture;
}

static int get_server_buffer(ProcessStruct *process,
                             unsigned int client_buffer)
{
    unsigned int server_buffer = 0;

    if (client_buffer < 32768) {
        server_buffer = process->current_state->tabBuffers[client_buffer];
    } else {
        fprintf(stderr, "invalid buffer name %d\n", client_buffer);
    }
    return server_buffer;
}


static int get_server_list(ProcessStruct *process, unsigned int client_list)
{
    unsigned int server_list = 0;

    if (client_list < 32768) {
        server_list = process->current_state->tabLists[client_list];
    } else {
        fprintf(stderr, "invalid list name %d\n", client_list);
    }
    return server_list;
}

GLXFBConfig get_fbconfig(ProcessStruct *process, int client_fbconfig)
{
    int i;
    int nbtotal = 0;

    for (i = 0; i < process->nfbconfig; i++) {
        assert(client_fbconfig >= 1 + nbtotal);
        if (client_fbconfig <= nbtotal + process->fbconfigs_max[i]) {
            return process->fbconfigs[i][client_fbconfig - 1 - nbtotal];
        }
        nbtotal += process->fbconfigs_max[i];
    }
    return 0;
}

typedef struct {
    int attribListLength;
    int *attribList;
    XVisualInfo *visInfo;
} AssocAttribListVisual;

static int nTabAssocAttribListVisual = 0;
static AssocAttribListVisual *tabAssocAttribListVisual = NULL;

static int _compute_length_of_attrib_list_including_zero(const int *attribList,
                                                         int
                                                         booleanMustHaveValue)
{
    int i = 0;

    while (attribList[i]) {
        if (booleanMustHaveValue ||
            !(attribList[i] == GLX_USE_GL || attribList[i] == GLX_RGBA ||
              attribList[i] == GLX_DOUBLEBUFFER ||
              attribList[i] == GLX_STEREO)) {
            i += 2;
        } else {
            i++;
        }
    }
    return i + 1;
}

static int glXChooseVisualFunc(Display *dpy, const int *_attribList)
{
    if (_attribList == NULL)
        return 0;
    int attribListLength =
        _compute_length_of_attrib_list_including_zero(_attribList, 0);
    int i;

    int *attribList = malloc(sizeof(int) * attribListLength);
    memcpy(attribList, _attribList, sizeof(int) * attribListLength);

    i = 0;
    while (attribList[i]) {
        if (!
            (attribList[i] == GLX_USE_GL || attribList[i] == GLX_RGBA ||
             attribList[i] == GLX_DOUBLEBUFFER ||
             attribList[i] == GLX_STEREO)) {
            if (attribList[i] == GLX_SAMPLE_BUFFERS && attribList[i + 1] != 0
                && getenv("DISABLE_SAMPLE_BUFFERS")) {
                fprintf(stderr, "Disabling GLX_SAMPLE_BUFFERS\n");
                attribList[i + 1] = 0;
            }
            i += 2;
        } else {
            i++;
        }
    }

    for (i = 0; i < nTabAssocAttribListVisual; i++) {
        if (tabAssocAttribListVisual[i].attribListLength == attribListLength
            && memcmp(tabAssocAttribListVisual[i].attribList, attribList,
                      attribListLength * sizeof(int)) == 0) {
            free(attribList);
            return (tabAssocAttribListVisual[i].
                    visInfo) ? tabAssocAttribListVisual[i].visInfo->
                visualid : 0;
        }
    }
    XVisualInfo *visInfo = glXChooseVisual(dpy, 0, attribList);

    tabAssocAttribListVisual = realloc(
                    tabAssocAttribListVisual, sizeof(AssocAttribListVisual) *
                    (nTabAssocAttribListVisual + 1));
    tabAssocAttribListVisual[nTabAssocAttribListVisual].attribListLength =
        attribListLength;
    tabAssocAttribListVisual[nTabAssocAttribListVisual].attribList =
        (int *) malloc(sizeof(int) * attribListLength);
    memcpy(tabAssocAttribListVisual[nTabAssocAttribListVisual].attribList,
           attribList, sizeof(int) * attribListLength);
    tabAssocAttribListVisual[nTabAssocAttribListVisual].visInfo = visInfo;
    nTabAssocAttribListVisual++;
    free(attribList);
    return (visInfo) ? visInfo->visualid : 0;
}

static XVisualInfo *get_visual_info_from_visual_id(Display *dpy,
                                                   int visualid)
{
    int i, n;
    XVisualInfo template;
    XVisualInfo *visInfo;

    for (i = 0; i < nTabAssocAttribListVisual; i++) {
        if (tabAssocAttribListVisual[i].visInfo &&
            tabAssocAttribListVisual[i].visInfo->visualid == visualid) {
            return tabAssocAttribListVisual[i].visInfo;
        }
    }
    template.visualid = visualid;
    visInfo = XGetVisualInfo(dpy, VisualIDMask, &template, &n);
    tabAssocAttribListVisual =
        realloc(tabAssocAttribListVisual,
                sizeof(AssocAttribListVisual) * (nTabAssocAttribListVisual +
                                                 1));
    tabAssocAttribListVisual[nTabAssocAttribListVisual].attribListLength = 0;
    tabAssocAttribListVisual[nTabAssocAttribListVisual].attribList = NULL;
    tabAssocAttribListVisual[nTabAssocAttribListVisual].visInfo = visInfo;
    nTabAssocAttribListVisual++;
    return visInfo;
}

typedef struct {
    int x;
    int y;
    int width;
    int height;
    int xhot;
    int yhot;
    int *pixels;
} ClientCursor;

#if 0
static ClientCursor client_cursor = { 0 };
#endif

static void do_glClientActiveTextureARB(int texture)
{
    GET_EXT_PTR_NO_FAIL(void, glClientActiveTextureARB, (int));

    if (ptr_func_glClientActiveTextureARB) {
        ptr_func_glClientActiveTextureARB(texture);
    }
}

#ifdef CURSOR_TRICK
static void do_glActiveTextureARB(int texture)
{
    GET_EXT_PTR_NO_FAIL(void, glActiveTextureARB, (int));

    if (ptr_func_glActiveTextureARB) {
        ptr_func_glActiveTextureARB(texture);
    }
}

static void do_glUseProgramObjectARB(GLhandleARB programObj)
{
    GET_EXT_PTR_NO_FAIL(void, glUseProgramObjectARB, (GLhandleARB));

    if (ptr_func_glUseProgramObjectARB) {
        ptr_func_glUseProgramObjectARB(programObj);
    }
}
#endif

static void destroy_gl_state(GLState *state)
{
    int i;

    if (state->vertexPointer)
        free(state->vertexPointer);
    if (state->normalPointer)
        free(state->normalPointer);
    if (state->indexPointer)
        free(state->indexPointer);
    if (state->colorPointer)
        free(state->colorPointer);
    if (state->secondaryColorPointer)
        free(state->secondaryColorPointer);
    for (i = 0; i < NB_MAX_TEXTURES; i++) {
        if (state->texCoordPointer[i])
            free(state->texCoordPointer[i]);
    }
    for (i = 0; i < MY_GL_MAX_VERTEX_ATTRIBS_ARB; i++) {
        if (state->vertexAttribPointer[i])
            free(state->vertexAttribPointer[i]);
    }
    for (i = 0; i < MY_GL_MAX_VERTEX_ATTRIBS_NV; i++) {
        if (state->vertexAttribPointerNV[i])
            free(state->vertexAttribPointerNV[i]);
    }
    if (state->weightPointer)
        free(state->weightPointer);
    if (state->matrixIndexPointer)
        free(state->matrixIndexPointer);
    if (state->fogCoordPointer)
        free(state->fogCoordPointer);
    for (i = 0; i < MY_GL_MAX_VARIANT_POINTER_EXT; i++) {
        if (state->variantPointerEXT[i])
            free(state->variantPointerEXT[i]);
    }
    if (state->interleavedArrays)
        free(state->interleavedArrays);
    if (state->elementPointerATI)
        free(state->elementPointerATI);
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

void _create_context(ProcessStruct *process, GLXContext ctxt, int fake_ctxt,
                     GLXContext shareList, int fake_shareList)
{
    process->glstates =
        realloc(process->glstates,
                (process->nb_states + 1) * sizeof(GLState *));
    process->glstates[process->nb_states] = malloc(sizeof(GLState));
    memset(process->glstates[process->nb_states], 0, sizeof(GLState));
    process->glstates[process->nb_states]->ref = 1;
    process->glstates[process->nb_states]->context = ctxt;
    process->glstates[process->nb_states]->fake_ctxt = fake_ctxt;
    process->glstates[process->nb_states]->fake_shareList = fake_shareList;
    init_gl_state(process->glstates[process->nb_states]);
    if (shareList && fake_shareList) {
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
}

static ProcessStruct *process;

void do_disconnect_current(void)
{
    int i;
    Display *dpy = process->dpy;

    glXMakeCurrent(dpy, 0, NULL);

    for (i = 0; i < MAX_ASSOC_SIZE &&
                    process->association_fakecontext_glxcontext[i].key; i ++) {
        GLXContext ctxt = process->association_fakecontext_glxcontext[i].value;

        fprintf(stderr, "Destroy context corresponding to fake_context"
                        " = %ld\n", (long) process->
                        association_fakecontext_glxcontext[i].key);
        glXDestroyContext(dpy, ctxt);
    }

    GET_EXT_PTR(void, glXDestroyPbuffer, (Display *, GLXPbuffer));

    for (i = 0; i < MAX_ASSOC_SIZE &&
                    process->association_fakepbuffer_pbuffer[i].key; i ++) {
        GLXPbuffer pbuffer = (GLXPbuffer)
                process->association_fakepbuffer_pbuffer[i].value;

        fprintf(stderr, "Destroy pbuffer corresponding to fake_pbuffer"
                        " = %ld\n", (long) process->
                        association_fakepbuffer_pbuffer[i].key);
        if (!is_gl_vendor_ati(dpy))
            ptr_func_glXDestroyPbuffer(dpy, pbuffer);
    }

    for (i = 0; i < MAX_ASSOC_SIZE && process->
                    association_clientdrawable_serverdrawable[i].key; i ++) {
        Window win = (Window) process->
                association_clientdrawable_serverdrawable[i].value;

        fprintf(stderr, "Destroy window %x corresponding to client_drawable "
                        "= %p\n", (int) win, process->
                        association_clientdrawable_serverdrawable[i].key);

//FIXMEIM - the crasher is here somewhere.
        if (active_win == win)
            active_win = 0;

        XDestroyWindow(dpy, win);

#if 1
        int loop = 1;
        while (loop) {
            while (XPending(dpy) > 0) {
                XEvent event;

                XNextEvent(dpy, &event);
                switch (event.type) {
                case DestroyNotify:
                    {
                        if (((XDestroyWindowEvent *) &event)->window == win)
                            loop = 0;
                        break;
                    }
                }
            }
            break; /* TODO */
        }
#endif

    }

    for (i = 0; i < process->nb_states; i++) {
        destroy_gl_state(process->glstates[i]);
        free(process->glstates[i]);
    }
    destroy_gl_state(&process->default_state);
    free(process->glstates);

    if (process->cmdbuf)
        qemu_free(process->cmdbuf);

    for (i = 0; &processes[i] != process; i ++);
    memmove(&processes[i], &processes[i + 1],
                    (MAX_HANDLED_PROCESS - 1 - i) * sizeof(ProcessStruct));
}

static const int beginend_allowed[GL_N_CALLS] = {
#undef MAGIC_MACRO
#define MAGIC_MACRO(name) [name ## _func] = 1,
#include "gl_beginend.h"
};

void do_context_switch(Display *dpy, pid_t pid, int call)
{
    int i;

    /* Lookup a process stuct. If there isnt one associated with this pid
     * then we create one.
     * process->current_state contains info on which of the guests contexts is
     * current.
     */
    for (i = 0; i < MAX_HANDLED_PROCESS; i ++)
        if (processes[i].process_id == pid) {
            process = &processes[i];
            break;
        } else if (processes[i].process_id == 0) {
            process = &processes[i];
            memset(process, 0, sizeof(ProcessStruct));
            process->process_id = pid;
            init_gl_state(&process->default_state);
            process->current_state = &process->default_state;
            process->dpy = dpy;
            break;
        }
    if (process == NULL) {
        fprintf(stderr, "Too many processes !\n");
        exit(-1);
    }

    switch (call) {
    case _init32_func:
    case _init64_func:
    case _exit_process_func:
    case glXMakeCurrent_func:
        /* Do nothing. In the case of glXMakeCurrent_func this avoids doing
           two calls to glXMakeCurrent() on the host because the context
           switch happens before the function call is processed */
        break;

    default:
            glXMakeCurrent(dpy, process->current_state->drawable,
                            process->current_state->context);
    }
}

int do_function_call(int func_number, arg_t *args, char *ret_string)
{
    union gl_ret_type ret;
    Display *dpy = process->dpy;
    Signature *signature = (Signature *) tab_opengl_calls[func_number];
    int ret_type = signature->ret_type;
    arg_t *tmp_args = args;

    ret.s = NULL;

    if (display_function_call) {
        fprintf(stderr, "[%d]> %s\n", process->p.process_id,
                tab_opengl_calls_name[func_number]);
	while(*tmp_args) {
		fprintf(stderr, " + %08x\n", *tmp_args++);
        }
    }

#ifdef BUFFER_BEGINEND
    if (process->began) {
        /* Need to check for any errors now because later we won't have
         * a chance to report them.  */
        if (beginend_allowed[func_number]) {
            arg_t *buf = cmd_buffer_alloc(process, signature->nb_args + 1);

            /* TODO: pointer arguments */
            buf[0] = func_number;
            memcpy(buf + 1, args, signature->nb_args * sizeof(arg_t));
        } else if (likely(func_number == glEnd_func)) {
            process->began = 0;

            glBegin(process->primitive);
            cmd_buffer_replay(process);
            glEnd();
        } else {
            /* TODO: properly report */
#ifdef SYSTEMATIC_ERROR_CHECK
            process->current_state->last_error = INVALID_OPERATION;
#endif
        }

        func_number = -1;
    }
#endif


    switch (func_number) {
    case -1:
        break;

    case _synchronize_func:
        ret.i = 1;
        break;

    case _exit_process_func:
        do_disconnect_current();
        break;

    case _changeWindowState_func:
        {
            ClientGLXDrawable client_drawable = to_drawable(args[0]);

            if (display_function_call)
                fprintf(stderr, "client_drawable=%p\n",
                        (void *) client_drawable);

            GLXDrawable drawable =
                    get_association_clientdrawable_serverdrawable(
                                    process, client_drawable);
            if (drawable) {
                if (args[1] == IsViewable) {
                    WindowPosStruct pos;

                    _get_window_pos(dpy, drawable, &pos);
                    if (pos.map_state != args[1]) {
                        XMapWindow(dpy, drawable);

                        int loop = 1;   // 1;

                        while (loop) {
                            while (XPending(dpy) > 0) {
                                XEvent event;

                                XNextEvent(dpy, &event);
                                switch (event.type) {
                                case ConfigureNotify:
                                    {
                                        if (((XConfigureEvent *) &event)->
                                            window == drawable) {
                                            loop = 0;
                                        }
                                        break;
                                    }
                                }
                            }
                            break; /* TODO */
                        }
                    }
                }
            }

            break;
        }

    case _moveResizeWindow_func:
        {
            int *params = (int *) args[1];
            ClientGLXDrawable client_drawable = to_drawable(args[0]);

            if (display_function_call)
                fprintf(stderr, "client_drawable=%p\n",
                        (void *) client_drawable);

            GLXDrawable drawable =
                    get_association_clientdrawable_serverdrawable(
                                    process, client_drawable);
            if (drawable) {
                WindowPosStruct pos;

                _get_window_pos(dpy, drawable, &pos);
                if (!
                    (params[0] == pos.x && params[1] == pos.y &&
                     params[2] == pos.width && params[3] == pos.height)) {
                    int redim = !(params[2] == pos.width &&
                                  params[3] == pos.height);
                    active_win_x = params[0];
                    active_win_y = params[1];

//                    fprintf(stderr, "old x=%d y=%d width=%d height=%d\n",
//                            pos.x, pos.y, pos.width, pos.height);
//                    fprintf(stderr, "new x=%d y=%d width=%d height=%d\n",
//                            params[0], params[1], params[2], params[3]);
                    XMoveResizeWindow(dpy, drawable, params[0], params[1],
                                      params[2], params[3]);

#if 0
                    int loop = 0;       // = 1

                    while (loop) {
                        while (XPending(dpy) > 0) {
                            XEvent event;

                            XNextEvent(dpy, &event);
                            switch (event.type) {
                            case ConfigureNotify:
                                {
                                    if (((XConfigureEvent *) &event)->
                                        window == drawable) {
                                        loop = 0;
                                    }
                                    break;
                                }
                            }
                        }
                    }
#endif
                    /* The window should have resized by now, but force the
                     * new size anyway.  */
                    _get_window_pos(dpy, drawable, &pos);
                    pos.width = params[2];
                    pos.height = params[3];
                    process->currentDrawablePos = pos;
                    // if (getenv("FORCE_GL_VIEWPORT"))
                    if (redim)
                        glViewport(0, 0, pos.width, pos.height);
                }
            }
            break;
        }

    case _send_cursor_func:
        {
#if 0
            int x = args[0];
            int y = args[1];
            int width = args[2];
            int height = args[3];
            int xhot = args[4];
            int yhot = args[5];
            int *pixels = (int *) args[6];

            client_cursor.x = x;
            client_cursor.y = y;
            client_cursor.width = width;
            client_cursor.height = height;
            client_cursor.xhot = xhot;
            client_cursor.yhot = yhot;
            if (pixels) {
                client_cursor.pixels =
                    realloc(client_cursor.pixels,
                            client_cursor.width * client_cursor.height *
                            sizeof(int));
                memcpy(client_cursor.pixels, pixels,
                       client_cursor.width * client_cursor.height *
                       sizeof(int));
            }
            int in_window = (x >= 0 && y >= 0 &&
                             x < process->currentDrawablePos.width &&
                             y < process->currentDrawablePos.height);
            // fprintf(stderr, "cursor at %d %d (%s)\n", x, y, (in_window) ?
            // "in window" : "not in window");
#endif
            break;
        }

#ifdef BUFFER_BEGINEND
    case glBegin_func:
        process->began = 1;
        process->primitive = args[0];
        break;
#endif

    case glXWaitGL_func:
        {
            glXWaitGL();
            ret.i = 0;
            break;
        }

    case glXWaitX_func:
        {
            glXWaitX();
            ret.i = 0;
            break;
        }

    case glXChooseVisual_func:
        {
            ret.i = glXChooseVisualFunc(dpy, (int *) args[2]);
            break;
        }

    case glXQueryExtensionsString_func:
        {
            ret.s = glXQueryExtensionsString(dpy, 0);
            break;
        }

    case glXQueryServerString_func:
        {
            ret.s = glXQueryServerString(dpy, 0, args[2]);
            break;
        }

    case glXGetClientString_func:
        {
            ret.s = glXGetClientString(dpy, args[1]);
            break;
        }

    case glXGetScreenDriver_func:
        {
            GET_EXT_PTR(const char *, glXGetScreenDriver, (Display *, int));

            ret.s = ptr_func_glXGetScreenDriver(dpy, 0);
            break;
        }

    case glXGetDriverConfig_func:
        {
            GET_EXT_PTR(const char *, glXGetDriverConfig, (const char *));

            ret.s = ptr_func_glXGetDriverConfig((const char *) args[0]);
            break;
        }

    case glXCreateContext_func:
        {
            int visualid = (int) args[1];
            int fake_shareList = (int) args[2];

            if (1 || display_function_call)
                fprintf(stderr, "visualid=%d, fake_shareList=%d\n", visualid,
                        fake_shareList);

            GLXContext shareList = get_association_fakecontext_glxcontext(
                            process, fake_shareList);
            XVisualInfo *vis = get_visual_info_from_visual_id(dpy, visualid);
            GLXContext ctxt;

            if (vis) {
                ctxt = glXCreateContext(dpy, vis, shareList, args[3]);
            } else {
                vis = get_default_visual(dpy);
                int saved_visualid = vis->visualid;

                vis->visualid = visualid ?: saved_visualid;
                ctxt = glXCreateContext(dpy, vis, shareList, args[3]);
                vis->visualid = saved_visualid;
            }
fprintf(stderr, "glXCreateContext: %08x %08x %08x\n", dpy, ctxt, vis);

            if (ctxt) {
                int fake_ctxt =++ process->next_available_context_number;

                set_association_fakecontext_visual(process, fake_ctxt, vis);
                set_association_fakecontext_glxcontext(process, fake_ctxt,
                                                       ctxt);
                ret.i = fake_ctxt;

                _create_context(process, ctxt, fake_ctxt, shareList,
                                fake_shareList);
            } else {
                ret.i = 0;
            }

            break;
        }


    case glXCreateNewContext_func:
        {
            GET_EXT_PTR(GLXContext, glXCreateNewContext,
                        (Display *, GLXFBConfig, int, GLXContext, int));
            int client_fbconfig = args[1];

            ret.i = 0;
            GLXFBConfig fbconfig = get_fbconfig(process, client_fbconfig);

            if (fbconfig) {
                int fake_shareList = args[3];
                GLXContext shareList = get_association_fakecontext_glxcontext(
                                process, fake_shareList);
                process->next_available_context_number++;
                int fake_ctxt = process->next_available_context_number;
                GLXContext ctxt = ptr_func_glXCreateNewContext(
                                dpy, fbconfig, args[2], shareList, args[4]);
                set_association_fakecontext_glxcontext(
                                process, fake_ctxt, ctxt);
                ret.i = fake_ctxt;

                _create_context(process, ctxt, fake_ctxt, shareList,
                                fake_shareList);
            }
            break;
        }

    case glXCopyContext_func:
        {
            int fake_src_ctxt = (int) args[1];
            int fake_dst_ctxt = (int) args[2];
            GLXContext src_ctxt;
            GLXContext dst_ctxt;

            if (display_function_call)
                fprintf(stderr, "fake_src_ctxt=%i, fake_dst_ctxt=%i\n",
                        fake_src_ctxt, fake_dst_ctxt);

            if (!(src_ctxt = get_association_fakecontext_glxcontext(
                                            process, fake_src_ctxt)))
                fprintf(stderr, "invalid fake_src_ctxt (%i) !\n",
                                fake_src_ctxt);
            else
                if (!(dst_ctxt = get_association_fakecontext_glxcontext(
                                                process, fake_dst_ctxt))) {
                fprintf(stderr, "invalid fake_dst_ctxt (%i) !\n",
                                fake_dst_ctxt);
            } else
                glXCopyContext(dpy, src_ctxt, dst_ctxt, args[3]);

            break;
        }

    case glXDestroyContext_func:
        {
            int fake_ctxt = (int) args[1];

            if (display_function_call)
                fprintf(stderr, "fake_ctxt=%d\n", fake_ctxt);

            GLXContext ctxt = get_association_fakecontext_glxcontext(
                            process, fake_ctxt);
            if (ctxt == NULL) {
                fprintf(stderr, "invalid fake_ctxt (%p) !\n",
                        (void *) (long) fake_ctxt);
            } else {
                int i;

                for (i = 0; i < process->nb_states; i ++) {
                    if (process->glstates[i]->fake_ctxt == fake_ctxt) {
                        if (ctxt == process->current_state->context)
                            process->current_state = &process->default_state;

                        int fake_shareList =
                            process->glstates[i]->fake_shareList;
                        process->glstates[i]->ref--;
                        if (process->glstates[i]->ref == 0) {
                            fprintf(stderr,
                                    "destroy_gl_state fake_ctxt = %d\n",
                                    process->glstates[i]->fake_ctxt);
                            destroy_gl_state(process->glstates[i]);
                            free(process->glstates[i]);
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
                                        fprintf(stderr,
                                                "destroy_gl_state fake_ctxt = %d\n",
                                                process->glstates[i]->
                                                fake_ctxt);
                                        destroy_gl_state(process->
                                                         glstates[i]);
                                        free(process->glstates[i]);
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

                        glXDestroyContext(dpy, ctxt);
                        unset_association_fakecontext_glxcontext(
                                        process, fake_ctxt);

                        break;
                    }
                }
            }
            break;
        }

    case glXQueryVersion_func:
        {
            ret.i = glXQueryVersion(dpy, (int *) args[1], (int *) args[2]);
            break;
        }

    case glGetString_func:
        {
            ret.s = (char *) glGetString(args[0]);
            break;
        }

    case glXMakeCurrent_func:
        {
            int i;
            ClientGLXDrawable client_drawable = to_drawable(args[1]);
            GLXDrawable host_drawable = 0;
            int fake_ctxt = (int) args[2];

            if (display_function_call)
                fprintf(stderr, "client_drawable=%p fake_ctx=%d\n",
                        (void *) client_drawable, fake_ctxt);

            if (client_drawable == 0 && fake_ctxt == 0) {

                /* Release context */

                ret.i = glXMakeCurrent(dpy, 0, NULL);
                process->current_state = &process->default_state;
            } else if ((host_drawable = (GLXDrawable)
                                    get_association_fakepbuffer_pbuffer(
                                            process, client_drawable))) {

                /* Lookup context if we have a valid drawable in the form of
                 * a pixel buffer (pbuffer).
                 * If it exists, use it. */

                GLXContext ctxt = get_association_fakecontext_glxcontext(
                                process, fake_ctxt);
                if (ctxt == NULL) {
                    fprintf(stderr, "invalid fake_ctxt (%d) (*)!\n",
                                    fake_ctxt);
                    ret.i = 0;
                } else
                    ret.i = glXMakeCurrent(dpy, host_drawable, ctxt);
            } else {

                /* Create an ordinary drawable if we have a valid context */

                GLXContext ctxt = get_association_fakecontext_glxcontext(
                                process, fake_ctxt);
                if (ctxt == NULL) {
                    fprintf(stderr, "invalid fake_ctxt (%d)!\n", fake_ctxt);
                    ret.i = 0;
                } else {
                    host_drawable = get_association_clientdrawable_serverdrawable(
                                    process, client_drawable);
                    if (host_drawable == 0) {
                        XVisualInfo *vis = get_association_fakecontext_visual(
                                        process, fake_ctxt);
                        if (vis == NULL)
                            vis = get_default_visual(dpy);
                        host_drawable = create_window(dpy, vis);

                        fprintf(stderr, "Create drawable: %16x %16lx\n", (unsigned int)host_drawable, (unsigned long int)client_drawable);

                        set_association_clientdrawable_serverdrawable(process,
                                        client_drawable, host_drawable);
                    }

                    ret.i = glXMakeCurrent(dpy, host_drawable, ctxt);
                }
            }

            if (ret.i) {
                for (i = 0; i < process->nb_states; i ++) {
                    if (process->glstates[i]->fake_ctxt == fake_ctxt) {
                        /* HACK !!! REMOVE */
                        process->current_state = process->glstates[i];
                        process->current_state->drawable = host_drawable;
                        break;
                    }
                }

                if (i == process->nb_states) {
                    fprintf(stderr, "error remembering the new context\n");
                    exit(-1);
                }
            }
            break;
        }

    case glXSwapBuffers_func:
        {
            ClientGLXDrawable client_drawable = to_drawable(args[1]);

            if (display_function_call)
                fprintf(stderr, "client_drawable=%p\n", client_drawable);

            GLXDrawable drawable =
                get_association_clientdrawable_serverdrawable(
                                process, client_drawable);
            if (!drawable) {
                fprintf(stderr, "unknown client_drawable (%p) !\n",
                        (void *) client_drawable);
            } else {
#ifdef CURSOR_TRICK
                if (client_cursor.pixels && local_connection == 0) {
                    glPushAttrib(GL_ALL_ATTRIB_BITS);
                    glPushClientAttrib(GL_ALL_ATTRIB_BITS);

                    glMatrixMode(GL_PROJECTION);
                    glPushMatrix();
                    glLoadIdentity();
                    glOrtho(0, process->currentDrawablePos.width,
                            process->currentDrawablePos.height, 0, -1, 1);
                    glMatrixMode(GL_MODELVIEW);
                    glPushMatrix();
                    glLoadIdentity();
                    glPixelZoom(1, -1);

                    glPixelStorei(GL_UNPACK_SKIP_PIXELS, 0);
                    glPixelStorei(GL_UNPACK_SKIP_ROWS, 0);
                    glPixelStorei(GL_UNPACK_SWAP_BYTES, 0);
                    glPixelStorei(GL_UNPACK_LSB_FIRST, 0);
                    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
                    glShadeModel(GL_SMOOTH);

                    glEnable(GL_BLEND);
                    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
                    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

                    int i, numUnits;

                    glGetIntegerv(GL_MAX_TEXTURE_UNITS_ARB, &numUnits);
                    for (i = 0; i < numUnits; i++) {
                        do_glActiveTextureARB(GL_TEXTURE0_ARB + i);
                        glDisable(GL_TEXTURE_1D);
                        glDisable(GL_TEXTURE_2D);
                        glDisable(GL_TEXTURE_3D);
                    }
                    glDisable(GL_ALPHA_TEST);
                    glDisable(GL_DEPTH_TEST);
                    glDisable(GL_STENCIL_TEST);
                    glDisable(GL_SCISSOR_TEST);
                    glDisable(GL_FRAGMENT_PROGRAM_ARB);
                    glDisable(GL_VERTEX_PROGRAM_ARB);
                    do_glUseProgramObjectARB(0);

                    // memset(client_cursor.pixels, 255, client_cursor.width
                    // * client_cursor.height * sizeof(int));

                    glRasterPos2d(client_cursor.x - client_cursor.xhot,
                                  client_cursor.y - client_cursor.yhot);
                    glDrawPixels(client_cursor.width, client_cursor.height,
                                 GL_BGRA, GL_UNSIGNED_BYTE,
                                 client_cursor.pixels);

                    glMatrixMode(GL_MODELVIEW);
                    glPopMatrix();

                    glMatrixMode(GL_PROJECTION);
                    glPopMatrix();

                    glPopClientAttrib();
                    glPopAttrib();
                }
#endif

                ////////// HORRIBLE HORRIBLE HACK
                if (drawable != active_win && active_win) {
                    glXMakeCurrent(dpy, active_win,
                                    processes[0].current_state->context);
                    glXSwapBuffers(dpy, active_win);
                    glXMakeCurrent(dpy, process->current_state->drawable,
                                    process->current_state->context);
                }
                glXSwapBuffers(dpy, drawable);
            }
            break;
        }

    case glXIsDirect_func:
        {
            int fake_ctxt = (int) args[1];

            if (display_function_call)
                fprintf(stderr, "fake_ctx=%x\n", fake_ctxt);
            GLXContext ctxt =
                get_association_fakecontext_glxcontext(process, fake_ctxt);
            if (ctxt == NULL) {
                fprintf(stderr, "invalid fake_ctxt (%x) !\n", fake_ctxt);
                ret.c = False;
            } else {
                ret.c = glXIsDirect(dpy, ctxt);
            }
            break;
        }

    case glXGetConfig_func:
        {
            int visualid = args[1];
            XVisualInfo *vis = NULL;

            if (visualid)
                vis = get_visual_info_from_visual_id(dpy, visualid);
            if (vis == NULL)
                vis = get_default_visual(dpy);
            ret.i = glXGetConfig(dpy, vis, args[2], (int *) args[3]);
            break;
        }

    case glXGetConfig_extended_func:
        {
            int visualid = args[1];
            int n = args[2];
            int i;
            XVisualInfo *vis = NULL;
            int *attribs = (int *) args[3];
            int *values = (int *) args[4];
            int *res = (int *) args[5];

            if (visualid)
                vis = get_visual_info_from_visual_id(dpy, visualid);
            if (vis == NULL)
                vis = get_default_visual(dpy);

            for (i = 0; i < n; i++) {
                res[i] = glXGetConfig(dpy, vis, attribs[i], &values[i]);
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
            ret.i =
                glXQueryExtension(dpy, (int *) args[1], (int *) args[2]);
            break;
        }

    case glXChooseFBConfig_func:
        {
            GET_EXT_PTR(GLXFBConfig *, glXChooseFBConfig,
                        (Display *, int, int *, int *));
            if (process->nfbconfig == MAX_FBCONFIG) {
                *(int *) args[3] = 0;
                ret.i = 0;
            } else {
                GLXFBConfig *fbconfigs =
                    ptr_func_glXChooseFBConfig(dpy, args[1], (int *) args[2],
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

    case glXGetFBConfigs_func:
        {
            GET_EXT_PTR(GLXFBConfig *, glXGetFBConfigs,
                        (Display *, int, int *));
            if (process->nfbconfig == MAX_FBCONFIG) {
                *(int *) args[2] = 0;
                ret.i = 0;
            } else {
                GLXFBConfig *fbconfigs =
                    ptr_func_glXGetFBConfigs(dpy, args[1], (int *) args[2]);
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
                fprintf(stderr, "glXCreatePbuffer --> %x\n", (int) pbuffer);
                if (pbuffer) {
                    ClientGLXDrawable fake_pbuffer = to_drawable(
                                    ++ process->next_available_pbuffer_number);

                    set_association_fakepbuffer_pbuffer(
                                    process, fake_pbuffer, pbuffer);
                    fprintf(stderr,
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
                fprintf(stderr, "fake_pbuffer=%p\n", fake_pbuffer);

            GLXPbuffer pbuffer = get_association_fakepbuffer_pbuffer(
                            process, fake_pbuffer);
            if (pbuffer == 0) {
                fprintf(stderr, "invalid fake_pbuffer (%p) !\n",
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
                fprintf(stderr, "fake_pbuffer=%p\n", fake_pbuffer);

            GLXPbuffer pbuffer = get_association_fakepbuffer_pbuffer(
                            process, fake_pbuffer);
            if (pbuffer == 0) {
                fprintf(stderr, "invalid fake_pbuffer (%p)  !\n",
                        fake_pbuffer);
            } else {
                if (!is_gl_vendor_ati(dpy))
                    ptr_func_glXDestroyGLXPbufferSGIX(dpy, pbuffer);
                unset_association_fakepbuffer_pbuffer(process, fake_pbuffer);
            }
            break;
        }

    case glXBindTexImageATI_func:
        {
            GET_EXT_PTR(void, glXBindTexImageATI,
                        (Display *, GLXPbuffer, int));
            ClientGLXDrawable fake_pbuffer = to_drawable(args[1]);

            if (display_function_call)
                fprintf(stderr, "fake_pbuffer=%p\n",
                                fake_pbuffer);

            GLXPbuffer pbuffer = get_association_fakepbuffer_pbuffer(
                                process, fake_pbuffer);
            if (pbuffer == 0) {
                fprintf(stderr,
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
                fprintf(stderr, "fake_pbuffer=%d\n",
                                (int) (long) fake_pbuffer);

            GLXPbuffer pbuffer = get_association_fakepbuffer_pbuffer(
                            process, fake_pbuffer);
            if (pbuffer == 0) {
                fprintf(stderr,
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
                fprintf(stderr, "fake_pbuffer=%p\n", fake_pbuffer);

            GLXPbuffer pbuffer = get_association_fakepbuffer_pbuffer(
                            process, fake_pbuffer);
            if (pbuffer == 0) {
                fprintf(stderr,
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
                fprintf(stderr, "fake_pbuffer=%p\n", fake_pbuffer);

            GLXPbuffer pbuffer = get_association_fakepbuffer_pbuffer(
                            process, fake_pbuffer);
            if (pbuffer == 0) {
                fprintf(stderr,
                        "glXReleaseTexImageARB : invalid fake_pbuffer (%p) !\n",
                        fake_pbuffer);
                ret.i = 0;
            } else {
                ret.i =
                    ptr_func_glXReleaseTexImageARB(dpy, pbuffer, args[2]);
            }
            break;
        }

    case glXGetFBConfigAttrib_func:
        {
            GET_EXT_PTR(int, glXGetFBConfigAttrib,
                        (Display *, GLXFBConfig, int, int *));
            int client_fbconfig = args[1];

            ret.i = 0;
            GLXFBConfig fbconfig = get_fbconfig(process, client_fbconfig);

            if (fbconfig)
                ret.i =
                    ptr_func_glXGetFBConfigAttrib(dpy, fbconfig, args[2],
                                                  (int *) args[3]);
            break;
        }

    case glXGetFBConfigAttrib_extended_func:
        {
            GET_EXT_PTR(int, glXGetFBConfigAttrib,
                        (Display *, GLXFBConfig, int, int *));
            int client_fbconfig = args[1];
            int n = args[2];
            int i;
            int *attribs = (int *) args[3];
            int *values = (int *) args[4];
            int *res = (int *) args[5];
            GLXFBConfig fbconfig = get_fbconfig(process, client_fbconfig);

            for (i = 0; i < n; i++) {
                if (fbconfig) {
                    res[i] =
                        ptr_func_glXGetFBConfigAttrib(dpy, fbconfig,
                                                      attribs[i], &values[i]);
                } else {
                    res[i] = 0;
                }
            }
            break;
        }

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

    case glXQueryContext_func:
        {
            GET_EXT_PTR(int, glXQueryContext,
                        (Display *, GLXContext, int, int *));
            int fake_ctxt = (int) args[1];

            if (display_function_call)
                fprintf(stderr, "fake_ctx=%i\n", fake_ctxt);
            GLXContext ctxt =
                get_association_fakecontext_glxcontext(process, fake_ctxt);
            if (ctxt == NULL) {
                fprintf(stderr, "invalid fake_ctxt (%i) !\n", fake_ctxt);
                ret.i = 0;
            } else {
                ret.i =
                    ptr_func_glXQueryContext(dpy, ctxt, args[2],
                                             (int *) args[3]);
            }
            break;
        }

    case glXQueryDrawable_func:
        {
            GET_EXT_PTR(void, glXQueryDrawable,
                        (Display *, GLXDrawable, int, int *));
            ClientGLXDrawable client_drawable = to_drawable(args[1]);
            GLXDrawable drawable =
                    get_association_clientdrawable_serverdrawable(
                                    process, client_drawable);

            if (display_function_call)
                fprintf(stderr, "client_drawable=%p\n",
                                client_drawable);

            if (!drawable)
                fprintf(stderr, "invalid client_drawable (%p) !\n",
                                client_drawable);
            else
                ptr_func_glXQueryDrawable(dpy, drawable,
                                args[2], (int *) args[3]);

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

    case glXCreateContextWithConfigSGIX_func:
        {
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
            break;
        }

    case glXGetVisualFromFBConfig_func:
        {
            GET_EXT_PTR(XVisualInfo *, glXGetVisualFromFBConfig,
                        (Display *, GLXFBConfig));
            int client_fbconfig = args[1];

            ret.i = 0;
            GLXFBConfig fbconfig = get_fbconfig(process, client_fbconfig);

            if (fbconfig) {
                XVisualInfo *vis =
                    ptr_func_glXGetVisualFromFBConfig(dpy, fbconfig);
                ret.i = (vis) ? vis->visualid : 0;
                if (vis) {
                    tabAssocAttribListVisual =
                        realloc(tabAssocAttribListVisual,
                                sizeof(AssocAttribListVisual) *
                                (nTabAssocAttribListVisual + 1));
                    tabAssocAttribListVisual[nTabAssocAttribListVisual].
                        attribListLength = 0;
                    tabAssocAttribListVisual[nTabAssocAttribListVisual].
                        attribList = NULL;
                    tabAssocAttribListVisual[nTabAssocAttribListVisual].
                        visInfo = vis;
                    nTabAssocAttribListVisual++;
                }
                if (display_function_call)
                    fprintf(stderr, "visualid = %d\n", ret.i);
            }
            break;
        }

    case glXSwapIntervalSGI_func:
        {
            GET_EXT_PTR(int, glXSwapIntervalSGI, (int));

            ret.i = ptr_func_glXSwapIntervalSGI(args[0]);
            break;
        }

    case glXGetProcAddress_fake_func:
        {
            if (display_function_call)
                fprintf(stderr, "%s\n", (char *) args[0]);
            ret.i = glXGetProcAddressARB((const GLubyte *) args[0]) != NULL;
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

                result[i] =
                    glXGetProcAddressARB((const GLubyte *) huge_buffer) !=
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
            unsigned int *clientTabTextures = malloc(n * sizeof(int));
            unsigned int *serverTabTextures = malloc(n * sizeof(int));

            alloc_range(process->current_state->textureAllocator, n,
                        clientTabTextures);

            ptr_func_glGenTextures(n, serverTabTextures);
            for (i = 0; i < n; i++) {
                process->current_state->tabTextures[clientTabTextures[i]] =
                    serverTabTextures[i];
            }

            free(clientTabTextures);
            free(serverTabTextures);
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

            unsigned int *serverTabTextures = malloc(n * sizeof(int));

            for (i = 0; i < n; i++) {
                serverTabTextures[i] =
                    get_server_texture(process, clientTabTextures[i]);
            }
            ptr_func_glDeleteTextures(n, serverTabTextures);
            for (i = 0; i < n; i++) {
                process->current_state->tabTextures[clientTabTextures[i]] = 0;
            }
            free(serverTabTextures);
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
            int *new_lists = malloc(sizeof(int) * n);

            for (i = 0; i < n; i++) {
                new_lists[i] =
                    get_server_list(process, translate_id(i, type, lists));
            }
            glCallLists(n, GL_UNSIGNED_INT, new_lists);
            free(new_lists);
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
            unsigned int *clientTabBuffers = malloc(n * sizeof(int));
            unsigned int *serverTabBuffers = malloc(n * sizeof(int));

            alloc_range(process->current_state->bufferAllocator, n,
                        clientTabBuffers);

            ptr_func_glGenBuffersARB(n, serverTabBuffers);
            for (i = 0; i < n; i++) {
                process->current_state->tabBuffers[clientTabBuffers[i]] =
                    serverTabBuffers[i];
            }

            free(clientTabBuffers);
            free(serverTabBuffers);
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

            int *serverTabBuffers = malloc(n * sizeof(int));

            for (i = 0; i < n; i++) {
                serverTabBuffers[i] =
                    get_server_buffer(process, clientTabBuffers[i]);
            }
            ptr_func_glDeleteBuffersARB(n, serverTabBuffers);
            for (i = 0; i < n; i++) {
                process->current_state->tabBuffers[clientTabBuffers[i]] = 0;
            }
            free(serverTabBuffers);
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
            GLcharARB **tab_prog = malloc(size * sizeof(GLcharARB *));
            int *tab_length = (int *) args[3];

            for (i = 0; i < size; i++) {
                tab_prog[i] = ((GLcharARB *) args[2]) + acc_length;
                acc_length += tab_length[i];
            }
            ptr_func_glShaderSourceARB(args[0], args[1], tab_prog,
                                       tab_length);
            free(tab_prog);
            break;
        }

    case glShaderSource_fake_func:
        {
            GET_EXT_PTR(void, glShaderSource, (int, int, char **, void *));
            int size = args[1];
            int i;
            int acc_length = 0;
            GLcharARB **tab_prog = malloc(size * sizeof(GLcharARB *));
            int *tab_length = (int *) args[3];

            for (i = 0; i < size; i++) {
                tab_prog[i] = ((GLcharARB *) args[2]) + acc_length;
                acc_length += tab_length[i];
            }
            ptr_func_glShaderSource(args[0], args[1], tab_prog, tab_length);
            free(tab_prog);
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
                realloc(process->current_state->vertexPointer,
                        process->current_state->vertexPointerSize);
            memcpy(process->current_state->vertexPointer + offset,
                   (void *) args[5], bytes_size);
            /* fprintf(stderr, "glVertexPointer_fake_func size=%d, type=%d,
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
                realloc(process->current_state->normalPointer,
                        process->current_state->normalPointerSize);
            memcpy(process->current_state->normalPointer + offset,
                   (void *) args[4], bytes_size);
            // fprintf(stderr, "glNormalPointer_fake_func type=%d, stride=%d, 
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
                realloc(process->current_state->indexPointer,
                        process->current_state->indexPointerSize);
            memcpy(process->current_state->indexPointer + offset,
                   (void *) args[4], bytes_size);
            // fprintf(stderr, "glIndexPointer_fake_func type=%d, stride=%d,
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
                realloc(process->current_state->edgeFlagPointer,
                        process->current_state->edgeFlagPointerSize);
            memcpy(process->current_state->edgeFlagPointer + offset,
                   (void *) args[3], bytes_size);
            // fprintf(stderr, "glEdgeFlagPointer_fake_func stride = %d,
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
                realloc(process->current_state->vertexAttribPointer[index],
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
                realloc(process->current_state->vertexAttribPointerNV[index],
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
                realloc(process->current_state->colorPointer,
                        process->current_state->colorPointerSize);
            memcpy(process->current_state->colorPointer + offset,
                   (void *) args[5], bytes_size);
            // fprintf(stderr, "glColorPointer_fake_func bytes_size = %d\n",
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
                realloc(process->current_state->secondaryColorPointer,
                        process->current_state->secondaryColorPointerSize);
            memcpy(process->current_state->secondaryColorPointer + offset,
                   (void *) args[5], bytes_size);
            // fprintf(stderr, "glSecondaryColorPointer_fake_func bytes_size
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
                realloc(process->current_state->texCoordPointer[index],
                        process->current_state->texCoordPointerSize[index]);
            memcpy(process->current_state->texCoordPointer[index] + offset,
                   (void *) args[6], bytes_size);
            /* fprintf(stderr, "glTexCoordPointer_fake_func size=%d, type=%d, 
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
                realloc(process->current_state->weightPointer,
                        process->current_state->weightPointerSize);
            memcpy(process->current_state->weightPointer + offset,
                   (void *) args[5], bytes_size);
            /* fprintf(stderr, "glWeightPointerARB_fake_func size=%d,
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
                realloc(process->current_state->matrixIndexPointer,
                        process->current_state->matrixIndexPointerSize);
            memcpy(process->current_state->matrixIndexPointer + offset,
                   (void *) args[5], bytes_size);
            /* fprintf(stderr, "glMatrixIndexPointerARB_fake_func size=%d,
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
                realloc(process->current_state->fogCoordPointer,
                        process->current_state->fogCoordPointerSize);
            memcpy(process->current_state->fogCoordPointer + offset,
                   (void *) args[4], bytes_size);
            // fprintf(stderr, "glFogCoordPointer_fake_func type=%d,
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
                realloc(process->current_state->variantPointerEXT[id],
                        process->current_state->variantPointerEXTSize[id]);
            memcpy(process->current_state->variantPointerEXT[id] + offset,
                   (void *) args[5], bytes_size);
            // fprintf(stderr, "glVariantPointerEXT_fake_func[%d] type=%d,
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
                realloc(process->current_state->interleavedArrays,
                        process->current_state->interleavedArraysSize);
            memcpy(process->current_state->interleavedArrays + offset,
                   (void *) args[4], bytes_size);
            // fprintf(stderr, "glInterleavedArrays_fake_func format=%d,
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
                realloc(process->current_state->elementPointerATI,
                        process->current_state->elementPointerATISize);
            memcpy(process->current_state->elementPointerATI,
                   (void *) args[2], bytes_size);
            // fprintf(stderr, "glElementPointerATI_fake_func type=%d,
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
                realloc(process->current_state->texCoordPointer[0],
                        bytes_size);
            memcpy(process->current_state->texCoordPointer[0],
                   (void *) args[4], bytes_size);
            /* fprintf(stderr, "glTexCoordPointer01_fake_func size=%d,
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
                realloc(process->current_state->texCoordPointer[0],
                        bytes_size);
            memcpy(process->current_state->texCoordPointer[0],
                   (void *) args[4], bytes_size);
            /* fprintf(stderr, "glTexCoordPointer012_fake_func size=%d,
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
                realloc(process->current_state->vertexPointer, bytes_size);
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
                realloc(process->current_state->vertexPointer,
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
                realloc(process->current_state->vertexPointer,
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
                realloc(process->current_state->vertexPointer,
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
                realloc(process->current_state->vertexPointer,
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
                realloc(process->current_state->vertexPointer,
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
                realloc(process->current_state->vertexPointer,
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
                realloc(process->current_state->vertexPointer,
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
                realloc(process->current_state->vertexPointer,
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
                realloc(process->current_state->vertexPointer,
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

    case _glDrawRangeElements_buffer_func:
        {
            glDrawRangeElements(args[0], args[1], args[2], args[3], args[4],
                                (void *) args[5]);
            break;
        }

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
//            fprintf(stderr, "glGetIntegerv(%x)=%d\n", (int) args[0],
//                    *(int *) args[1]);
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
                realloc(process->current_state->selectBufferPtr,
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
                realloc(process->current_state->feedbackBufferPtr,
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
         * fprintf(stderr, "cap : %s\n", nameArrays[args[0] -
         * GL_VERTEX_ARRAY]); glEnableClientState(args[0]); break; }
         * 
         * case glDisableClientState_func: { if (display_function_call)
         * fprintf(stderr, "cap : %s\n", nameArrays[args[0] -
         * GL_VERTEX_ARRAY]); glDisableClientState(args[0]); break; }
         * 
         * case glClientActiveTexture_func: case
         * glClientActiveTextureARB_func: { if (display_function_call)
         * fprintf(stderr, "client activeTexture %d\n", args[0] -
         * GL_TEXTURE0_ARB); glClientActiveTextureARB(args[0]); break; }
         * 
         * case glActiveTextureARB_func: { if (display_function_call)
         * fprintf(stderr, "server activeTexture %d\n", args[0] -
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
#ifdef SYSTEMATIC_ERROR_CHECK
            ret.i = process->current_state->last_error;
#else
            ret.i = glGetError();
#endif
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

#ifdef SYSTEMATIC_ERROR_CHECK
    if (func_number == glGetError_func) {
        process->current_state->last_error = 0;
    } else {
        process->current_state->last_error = glGetError();
        if (process->current_state->last_error != 0) {
            printf("error %s 0x%x\n", tab_opengl_calls_name[func_number],
                   process->current_state->last_error);
        }
    }
#endif

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
        fprintf(stderr, "unexpected ret type : %d\n", ret_type);
        exit(-1);
        break;
    }

    if (display_function_call)
        fprintf(stderr, "[%d]< %s\n", process->p.process_id,
                tab_opengl_calls_name[func_number]);

    return ret.i;
}
