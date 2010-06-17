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


#define FGLX_VERSION_STRING "1.1"
#define FGLX_VERSION_MAJOR 1
#define FGLX_VERSION_MINOR 1


void *qemu_malloc(size_t size);
void *qemu_realloc(void *ptr, size_t size);
void qemu_free(void *ptr);

#define glGetError() 0

#ifdef _WIN32
#define GL_GETPROCADDRESS(X) wglGetProcAddressFunc(X)
#else
#define GL_GETPROCADDRESS(X) glXGetProcAddressARB(X)
#endif

#define GET_EXT_PTR(type, funcname, args_decl) \
      static int detect_##funcname = 0; \
      static type(*ptr_func_##funcname)args_decl = NULL; \
      if (detect_##funcname == 0) \
      { \
        detect_##funcname = 1; \
        ptr_func_##funcname = (type(*)args_decl)GL_GETPROCADDRESS((const GLubyte*)#funcname); \
        assert (ptr_func_##funcname); \
      }

#define GET_EXT_PTR_NO_FAIL(type, funcname, args_decl) \
      static int detect_##funcname = 0; \
      static type(*ptr_func_##funcname)args_decl = NULL; \
      if (detect_##funcname == 0) \
      { \
        detect_##funcname = 1; \
        ptr_func_##funcname = (type(*)args_decl)GL_GETPROCADDRESS((const GLubyte*)#funcname); \
      }

#ifndef WIN32
#include <dlfcn.h>
#endif


#ifdef _WIN32
static const char* KNOWN_GL_FUNCTIONS =
{
"glAccum\0"
"glActiveStencilFaceEXT\0"
"glActiveTexture\0"
"glActiveTextureARB\0"
"glActiveVaryingNV\0"
"glAddSwapHintRectWIN\0"
"glAlphaFragmentOp1ATI\0"
"glAlphaFragmentOp2ATI\0"
"glAlphaFragmentOp3ATI\0"
"glAlphaFunc\0"
"glApplyTextureEXT\0"
"glAreProgramsResidentNV\0"
"glAreTexturesResident\0"
"glAreTexturesResidentEXT\0"
"glArrayElement\0"
"glArrayElementEXT\0"
"glArrayObjectATI\0"
"glAsyncMarkerSGIX\0"
"glAttachObjectARB\0"
"glAttachShader\0"
"glBegin\0"
"glBeginConditionalRenderNVX\0"
"glBeginDefineVisibilityQueryATI\0"
"glBeginFragmentShaderATI\0"
"glBeginOcclusionQuery\0"
"glBeginOcclusionQueryNV\0"
"glBeginQuery\0"
"glBeginQueryARB\0"
"glBeginSceneEXT\0"
"glBeginTransformFeedbackNV\0"
"glBeginUseVisibilityQueryATI\0"
"glBeginVertexShaderEXT\0"
"glBindArraySetARB\0"
"glBindAttribLocation\0"
"glBindAttribLocationARB\0"
"glBindBuffer\0"
"glBindBufferARB\0"
"glBindBufferBaseNV\0"
"glBindBufferOffsetNV\0"
"glBindBufferRangeNV\0"
"glBindFragDataLocationEXT\0"
"glBindFragmentShaderATI\0"
"glBindFramebufferEXT\0"
"glBindLightParameterEXT\0"
"glBindMaterialParameterEXT\0"
"glBindParameterEXT\0"
"glBindProgramARB\0"
"glBindProgramNV\0"
"glBindRenderbufferEXT\0"
"glBindTexGenParameterEXT\0"
"glBindTexture\0"
"glBindTextureEXT\0"
"glBindTextureUnitParameterEXT\0"
"glBindVertexArrayAPPLE\0"
"glBindVertexShaderEXT\0"
"glBinormal3bEXT\0"
"glBinormal3bvEXT\0"
"glBinormal3dEXT\0"
"glBinormal3dvEXT\0"
"glBinormal3fEXT\0"
"glBinormal3fvEXT\0"
"glBinormal3iEXT\0"
"glBinormal3ivEXT\0"
"glBinormal3sEXT\0"
"glBinormal3svEXT\0"
"glBinormalArrayEXT\0"
"glBitmap\0"
"glBlendColor\0"
"glBlendColorEXT\0"
"glBlendEquation\0"
"glBlendEquationEXT\0"
"glBlendEquationSeparate\0"
"glBlendEquationSeparateATI\0"
"glBlendEquationSeparateEXT\0"
"glBlendFunc\0"
"glBlendFuncSeparate\0"
"glBlendFuncSeparateEXT\0"
"glBlendFuncSeparateINGR\0"
"glBlitFramebufferEXT\0"
"glBufferData\0"
"glBufferDataARB\0"
"glBufferParameteriAPPLE\0"
"glBufferRegionEnabled\0"
"glBufferRegionEnabledEXT\0"
"glBufferSubData\0"
"glBufferSubDataARB\0"
"glCallList\0"
"glCallLists\0"
"glCheckFramebufferStatusEXT\0"
"glClampColorARB\0"
"glClear\0"
"glClearAccum\0"
"glClearColor\0"
"glClearColorIiEXT\0"
"glClearColorIuiEXT\0"
"glClearDepth\0"
"glClearDepthdNV\0"
"glClearIndex\0"
"glClearStencil\0"
"glClientActiveTexture\0"
"glClientActiveTextureARB\0"
"glClientActiveVertexStreamATI\0"
"glClipPlane\0"
"glColor3b\0"
"glColor3bv\0"
"glColor3d\0"
"glColor3dv\0"
"glColor3f\0"
"glColor3fv\0"
"glColor3fVertex3fSUN\0"
"glColor3fVertex3fvSUN\0"
"glColor3hNV\0"
"glColor3hvNV\0"
"glColor3i\0"
"glColor3iv\0"
"glColor3s\0"
"glColor3sv\0"
"glColor3ub\0"
"glColor3ubv\0"
"glColor3ui\0"
"glColor3uiv\0"
"glColor3us\0"
"glColor3usv\0"
"glColor4b\0"
"glColor4bv\0"
"glColor4d\0"
"glColor4dv\0"
"glColor4f\0"
"glColor4fNormal3fVertex3fSUN\0"
"glColor4fNormal3fVertex3fvSUN\0"
"glColor4fv\0"
"glColor4hNV\0"
"glColor4hvNV\0"
"glColor4i\0"
"glColor4iv\0"
"glColor4s\0"
"glColor4sv\0"
"glColor4ub\0"
"glColor4ubv\0"
"glColor4ubVertex2fSUN\0"
"glColor4ubVertex2fvSUN\0"
"glColor4ubVertex3fSUN\0"
"glColor4ubVertex3fvSUN\0"
"glColor4ui\0"
"glColor4uiv\0"
"glColor4us\0"
"glColor4usv\0"
"glColorFragmentOp1ATI\0"
"glColorFragmentOp2ATI\0"
"glColorFragmentOp3ATI\0"
"glColorMask\0"
"glColorMaskIndexedEXT\0"
"glColorMaterial\0"
"glColorPointer\0"
"glColorPointerEXT\0"
"glColorPointerListIBM\0"
"glColorPointervINTEL\0"
"glColorSubTable\0"
"glColorSubTableEXT\0"
"glColorTable\0"
"glColorTableEXT\0"
"glColorTableParameterfv\0"
"glColorTableParameterfvSGI\0"
"glColorTableParameteriv\0"
"glColorTableParameterivSGI\0"
"glColorTableSGI\0"
"glCombinerInputNV\0"
"glCombinerOutputNV\0"
"glCombinerParameterfNV\0"
"glCombinerParameterfvNV\0"
"glCombinerParameteriNV\0"
"glCombinerParameterivNV\0"
"glCombinerStageParameterfvNV\0"
"glCompileShader\0"
"glCompileShaderARB\0"
"glCompressedTexImage1D\0"
"glCompressedTexImage1DARB\0"
"glCompressedTexImage2D\0"
"glCompressedTexImage2DARB\0"
"glCompressedTexImage3D\0"
"glCompressedTexImage3DARB\0"
"glCompressedTexSubImage1D\0"
"glCompressedTexSubImage1DARB\0"
"glCompressedTexSubImage2D\0"
"glCompressedTexSubImage2DARB\0"
"glCompressedTexSubImage3D\0"
"glCompressedTexSubImage3DARB\0"
"glConvolutionFilter1D\0"
"glConvolutionFilter1DEXT\0"
"glConvolutionFilter2D\0"
"glConvolutionFilter2DEXT\0"
"glConvolutionParameterf\0"
"glConvolutionParameterfEXT\0"
"glConvolutionParameterfv\0"
"glConvolutionParameterfvEXT\0"
"glConvolutionParameteri\0"
"glConvolutionParameteriEXT\0"
"glConvolutionParameteriv\0"
"glConvolutionParameterivEXT\0"
"glCopyColorSubTable\0"
"glCopyColorSubTableEXT\0"
"glCopyColorTable\0"
"glCopyColorTableSGI\0"
"glCopyConvolutionFilter1D\0"
"glCopyConvolutionFilter1DEXT\0"
"glCopyConvolutionFilter2D\0"
"glCopyConvolutionFilter2DEXT\0"
"glCopyPixels\0"
"glCopyTexImage1D\0"
"glCopyTexImage1DEXT\0"
"glCopyTexImage2D\0"
"glCopyTexImage2DEXT\0"
"glCopyTexSubImage1D\0"
"glCopyTexSubImage1DEXT\0"
"glCopyTexSubImage2D\0"
"glCopyTexSubImage2DEXT\0"
"glCopyTexSubImage3D\0"
"glCopyTexSubImage3DEXT\0"
"glCreateProgram\0"
"glCreateProgramObjectARB\0"
"glCreateShader\0"
"glCreateShaderObjectARB\0"
"glCullFace\0"
"glCullParameterdvEXT\0"
"glCullParameterfvEXT\0"
"glCurrentPaletteMatrixARB\0"
"glDeformationMap3dSGIX\0"
"glDeformationMap3fSGIX\0"
"glDeformSGIX\0"
"glDeleteArraySetsARB\0"
"glDeleteAsyncMarkersSGIX\0"
"glDeleteBufferRegion\0"
"glDeleteBufferRegionEXT\0"
"glDeleteBuffers\0"
"glDeleteBuffersARB\0"
"glDeleteFencesAPPLE\0"
"glDeleteFencesNV\0"
"glDeleteFragmentShaderATI\0"
"glDeleteFramebuffersEXT\0"
"glDeleteLists\0"
"glDeleteObjectARB\0"
"glDeleteObjectBufferATI\0"
"glDeleteOcclusionQueries\0"
"glDeleteOcclusionQueriesNV\0"
"glDeleteProgram\0"
"glDeleteProgramsARB\0"
"glDeleteProgramsNV\0"
"glDeleteQueries\0"
"glDeleteQueriesARB\0"
"glDeleteRenderbuffersEXT\0"
"glDeleteShader\0"
"glDeleteTextures\0"
"glDeleteTexturesEXT\0"
"glDeleteVertexArraysAPPLE\0"
"glDeleteVertexShaderEXT\0"
"glDeleteVisibilityQueriesATI\0"
"glDepthBoundsdNV\0"
"glDepthBoundsEXT\0"
"glDepthFunc\0"
"glDepthMask\0"
"glDepthRange\0"
"glDepthRangedNV\0"
"glDetachObjectARB\0"
"glDetachShader\0"
"glDetailTexFuncSGIS\0"
"glDisable\0"
"glDisableClientState\0"
"glDisableIndexedEXT\0"
"glDisableVariantClientStateEXT\0"
"glDisableVertexAttribAPPLE\0"
"glDisableVertexAttribArray\0"
"glDisableVertexAttribArrayARB\0"
"glDrawArrays\0"
"glDrawArraysEXT\0"
"glDrawArraysInstancedEXT\0"
"glDrawBuffer\0"
"glDrawBufferRegion\0"
"glDrawBufferRegionEXT\0"
"glDrawBuffers\0"
"glDrawBuffersARB\0"
"glDrawBuffersATI\0"
"glDrawElementArrayAPPLE\0"
"glDrawElementArrayATI\0"
"glDrawElements\0"
"glDrawElementsFGL\0"
"glDrawElementsInstancedEXT\0"
"glDrawMeshArraysSUN\0"
"glDrawMeshNV\0"
"glDrawPixels\0"
"glDrawRangeElementArrayAPPLE\0"
"glDrawRangeElementArrayATI\0"
"glDrawRangeElements\0"
"glDrawRangeElementsEXT\0"
"glDrawWireTrianglesFGL\0"
"glEdgeFlag\0"
"glEdgeFlagPointer\0"
"glEdgeFlagPointerEXT\0"
"glEdgeFlagPointerListIBM\0"
"glEdgeFlagv\0"
"glElementPointerAPPLE\0"
"glElementPointerATI\0"
"glEnable\0"
"glEnableClientState\0"
"glEnableIndexedEXT\0"
"glEnableVariantClientStateEXT\0"
"glEnableVertexAttribAPPLE\0"
"glEnableVertexAttribArray\0"
"glEnableVertexAttribArrayARB\0"
"glEnd\0"
"glEndConditionalRenderNVX\0"
"glEndDefineVisibilityQueryATI\0"
"glEndFragmentShaderATI\0"
"glEndList\0"
"glEndOcclusionQuery\0"
"glEndOcclusionQueryNV\0"
"glEndQuery\0"
"glEndQueryARB\0"
"glEndSceneEXT\0"
"glEndTransformFeedbackNV\0"
"glEndUseVisibilityQueryATI\0"
"glEndVertexShaderEXT\0"
"glEvalCoord1d\0"
"glEvalCoord1dv\0"
"glEvalCoord1f\0"
"glEvalCoord1fv\0"
"glEvalCoord2d\0"
"glEvalCoord2dv\0"
"glEvalCoord2f\0"
"glEvalCoord2fv\0"
"glEvalMapsNV\0"
"glEvalMesh1\0"
"glEvalMesh2\0"
"glEvalPoint1\0"
"glEvalPoint2\0"
"glExecuteProgramNV\0"
"glExtractComponentEXT\0"
"glFeedbackBuffer\0"
"glFinalCombinerInputNV\0"
"glFinish\0"
"glFinishAsyncSGIX\0"
"glFinishFenceAPPLE\0"
"glFinishFenceNV\0"
"glFinishObjectAPPLE\0"
"glFinishRenderAPPLE\0"
"glFinishTextureSUNX\0"
"glFlush\0"
"glFlushMappedBufferRangeAPPLE\0"
"glFlushPixelDataRangeNV\0"
"glFlushRasterSGIX\0"
"glFlushRenderAPPLE\0"
"glFlushVertexArrayRangeAPPLE\0"
"glFlushVertexArrayRangeNV\0"
"glFogCoordd\0"
"glFogCoorddEXT\0"
"glFogCoorddv\0"
"glFogCoorddvEXT\0"
"glFogCoordf\0"
"glFogCoordfEXT\0"
"glFogCoordfv\0"
"glFogCoordfvEXT\0"
"glFogCoordhNV\0"
"glFogCoordhvNV\0"
"glFogCoordPointer\0"
"glFogCoordPointerEXT\0"
"glFogCoordPointerListIBM\0"
"glFogf\0"
"glFogFuncSGIS\0"
"glFogfv\0"
"glFogi\0"
"glFogiv\0"
"glFragmentColorMaterialEXT\0"
"glFragmentColorMaterialSGIX\0"
"glFragmentLightfEXT\0"
"glFragmentLightfSGIX\0"
"glFragmentLightfvEXT\0"
"glFragmentLightfvSGIX\0"
"glFragmentLightiEXT\0"
"glFragmentLightiSGIX\0"
"glFragmentLightivEXT\0"
"glFragmentLightivSGIX\0"
"glFragmentLightModelfEXT\0"
"glFragmentLightModelfSGIX\0"
"glFragmentLightModelfvEXT\0"
"glFragmentLightModelfvSGIX\0"
"glFragmentLightModeliEXT\0"
"glFragmentLightModeliSGIX\0"
"glFragmentLightModelivEXT\0"
"glFragmentLightModelivSGIX\0"
"glFragmentMaterialfEXT\0"
"glFragmentMaterialfSGIX\0"
"glFragmentMaterialfvEXT\0"
"glFragmentMaterialfvSGIX\0"
"glFragmentMaterialiEXT\0"
"glFragmentMaterialiSGIX\0"
"glFragmentMaterialivEXT\0"
"glFragmentMaterialivSGIX\0"
"glFramebufferRenderbufferEXT\0"
"glFramebufferTexture1DEXT\0"
"glFramebufferTexture2DEXT\0"
"glFramebufferTexture3DEXT\0"
"glFramebufferTextureEXT\0"
"glFramebufferTextureFaceEXT\0"
"glFramebufferTextureLayerEXT\0"
"glFrameZoomSGIX\0"
"glFreeObjectBufferATI\0"
"glFrontFace\0"
"glFrustum\0"
"glGenArraySetsARB\0"
"glGenAsyncMarkersSGIX\0"
"glGenBuffers\0"
"glGenBuffersARB\0"
"glGenerateMipmapEXT\0"
"glGenFencesAPPLE\0"
"glGenFencesNV\0"
"glGenFragmentShadersATI\0"
"glGenFramebuffersEXT\0"
"glGenLists\0"
"glGenOcclusionQueries\0"
"glGenOcclusionQueriesNV\0"
"glGenProgramsARB\0"
"glGenProgramsNV\0"
"glGenQueries\0"
"glGenQueriesARB\0"
"glGenRenderbuffersEXT\0"
"glGenSymbolsEXT\0"
"glGenTextures\0"
"glGenTexturesEXT\0"
"glGenVertexArraysAPPLE\0"
"glGenVertexShadersEXT\0"
"glGenVisibilityQueriesATI\0"
"glGetActiveAttrib\0"
"glGetActiveAttribARB\0"
"glGetActiveUniform\0"
"glGetActiveUniformARB\0"
"glGetActiveVaryingNV\0"
"glGetArrayObjectfvATI\0"
"glGetArrayObjectivATI\0"
"glGetAttachedObjectsARB\0"
"glGetAttachedShaders\0"
"glGetAttribLocation\0"
"glGetAttribLocationARB\0"
"glGetBooleanIndexedvEXT\0"
"glGetBooleanv\0"
"glGetBufferParameteriv\0"
"glGetBufferParameterivARB\0"
"glGetBufferPointerv\0"
"glGetBufferPointervARB\0"
"glGetBufferSubData\0"
"glGetBufferSubDataARB\0"
"glGetClipPlane\0"
"glGetColorTable\0"
"glGetColorTableEXT\0"
"glGetColorTableParameterfv\0"
"glGetColorTableParameterfvEXT\0"
"glGetColorTableParameterfvSGI\0"
"glGetColorTableParameteriv\0"
"glGetColorTableParameterivEXT\0"
"glGetColorTableParameterivSGI\0"
"glGetColorTableSGI\0"
"glGetCombinerInputParameterfvNV\0"
"glGetCombinerInputParameterivNV\0"
"glGetCombinerOutputParameterfvNV\0"
"glGetCombinerOutputParameterivNV\0"
"glGetCombinerStageParameterfvNV\0"
"glGetCompressedTexImage\0"
"glGetCompressedTexImageARB\0"
"glGetConvolutionFilter\0"
"glGetConvolutionFilterEXT\0"
"glGetConvolutionParameterfv\0"
"glGetConvolutionParameterfvEXT\0"
"glGetConvolutionParameteriv\0"
"glGetConvolutionParameterivEXT\0"
"glGetDetailTexFuncSGIS\0"
"glGetDoublev\0"
"glGetError\0"
"glGetFenceivNV\0"
"glGetFinalCombinerInputParameterfvNV\0"
"glGetFinalCombinerInputParameterivNV\0"
"glGetFloatv\0"
"glGetFogFuncSGIS\0"
"glGetFragDataLocationEXT\0"
"glGetFragmentLightfvEXT\0"
"glGetFragmentLightfvSGIX\0"
"glGetFragmentLightivEXT\0"
"glGetFragmentLightivSGIX\0"
"glGetFragmentMaterialfvEXT\0"
"glGetFragmentMaterialfvSGIX\0"
"glGetFragmentMaterialivEXT\0"
"glGetFragmentMaterialivSGIX\0"
"glGetFramebufferAttachmentParameterivEXT\0"
"glGetHandleARB\0"
"glGetHistogram\0"
"glGetHistogramEXT\0"
"glGetHistogramParameterfv\0"
"glGetHistogramParameterfvEXT\0"
"glGetHistogramParameteriv\0"
"glGetHistogramParameterivEXT\0"
"glGetImageTransformParameterfvHP\0"
"glGetImageTransformParameterivHP\0"
"glGetInfoLogARB\0"
"glGetInstrumentsSGIX\0"
"glGetIntegerIndexedvEXT\0"
"glGetIntegerv\0"
"glGetInvariantBooleanvEXT\0"
"glGetInvariantFloatvEXT\0"
"glGetInvariantIntegervEXT\0"
"glGetLightfv\0"
"glGetLightiv\0"
"glGetListParameterfvSGIX\0"
"glGetListParameterivSGIX\0"
"glGetLocalConstantBooleanvEXT\0"
"glGetLocalConstantFloatvEXT\0"
"glGetLocalConstantIntegervEXT\0"
"glGetMapAttribParameterfvNV\0"
"glGetMapAttribParameterivNV\0"
"glGetMapControlPointsNV\0"
"glGetMapdv\0"
"glGetMapfv\0"
"glGetMapiv\0"
"glGetMapParameterfvNV\0"
"glGetMapParameterivNV\0"
"glGetMaterialfv\0"
"glGetMaterialiv\0"
"glGetMinmax\0"
"glGetMinmaxEXT\0"
"glGetMinmaxParameterfv\0"
"glGetMinmaxParameterfvEXT\0"
"glGetMinmaxParameteriv\0"
"glGetMinmaxParameterivEXT\0"
"glGetObjectBufferfvATI\0"
"glGetObjectBufferivATI\0"
"glGetObjectParameterfvARB\0"
"glGetObjectParameterivARB\0"
"glGetOcclusionQueryiv\0"
"glGetOcclusionQueryivNV\0"
"glGetOcclusionQueryuiv\0"
"glGetOcclusionQueryuivNV\0"
"glGetPixelMapfv\0"
"glGetPixelMapuiv\0"
"glGetPixelMapusv\0"
"glGetPixelTexGenParameterfvSGIS\0"
"glGetPixelTexGenParameterivSGIS\0"
"glGetPixelTransformParameterfvEXT\0"
"glGetPixelTransformParameterivEXT\0"
"glGetPointerv\0"
"glGetPointervEXT\0"
"glGetPolygonStipple\0"
"glGetProgramEnvParameterdvARB\0"
"glGetProgramEnvParameterfvARB\0"
"glGetProgramEnvParameterIivNV\0"
"glGetProgramEnvParameterIuivNV\0"
"glGetProgramInfoLog\0"
"glGetProgramiv\0"
"glGetProgramivARB\0"
"glGetProgramivNV\0"
"glGetProgramLocalParameterdvARB\0"
"glGetProgramLocalParameterfvARB\0"
"glGetProgramLocalParameterIivNV\0"
"glGetProgramLocalParameterIuivNV\0"
"glGetProgramNamedParameterdvNV\0"
"glGetProgramNamedParameterfvNV\0"
"glGetProgramParameterdvNV\0"
"glGetProgramParameterfvNV\0"
"glGetProgramRegisterfvMESA\0"
"glGetProgramStringARB\0"
"glGetProgramStringNV\0"
"glGetQueryiv\0"
"glGetQueryivARB\0"
"glGetQueryObjecti64vEXT\0"
"glGetQueryObjectiv\0"
"glGetQueryObjectivARB\0"
"glGetQueryObjectui64vEXT\0"
"glGetQueryObjectuiv\0"
"glGetQueryObjectuivARB\0"
"glGetRenderbufferParameterivEXT\0"
"glGetSeparableFilter\0"
"glGetSeparableFilterEXT\0"
"glGetShaderInfoLog\0"
"glGetShaderiv\0"
"glGetShaderSource\0"
"glGetShaderSourceARB\0"
"glGetSharpenTexFuncSGIS\0"
"glGetString\0"
"glGetTexBumpParameterfvATI\0"
"glGetTexBumpParameterivATI\0"
"glGetTexEnvfv\0"
"glGetTexEnviv\0"
"glGetTexFilterFuncSGIS\0"
"glGetTexGendv\0"
"glGetTexGenfv\0"
"glGetTexGeniv\0"
"glGetTexImage\0"
"glGetTexLevelParameterfv\0"
"glGetTexLevelParameteriv\0"
"glGetTexParameterfv\0"
"glGetTexParameterIivEXT\0"
"glGetTexParameterIuivEXT\0"
"glGetTexParameteriv\0"
"glGetTexParameterPointervAPPLE\0"
"glGetTrackMatrixivNV\0"
"glGetTransformFeedbackVaryingNV\0"
"glGetUniformBufferSizeEXT\0"
"glGetUniformfv\0"
"glGetUniformfvARB\0"
"glGetUniformiv\0"
"glGetUniformivARB\0"
"glGetUniformLocation\0"
"glGetUniformLocationARB\0"
"glGetUniformOffsetEXT\0"
"glGetUniformuivEXT\0"
"glGetVariantArrayObjectfvATI\0"
"glGetVariantArrayObjectivATI\0"
"glGetVariantBooleanvEXT\0"
"glGetVariantFloatvEXT\0"
"glGetVariantIntegervEXT\0"
"glGetVariantPointervEXT\0"
"glGetVaryingLocationNV\0"
"glGetVertexAttribArrayObjectfvATI\0"
"glGetVertexAttribArrayObjectivATI\0"
"glGetVertexAttribdv\0"
"glGetVertexAttribdvARB\0"
"glGetVertexAttribdvNV\0"
"glGetVertexAttribfv\0"
"glGetVertexAttribfvARB\0"
"glGetVertexAttribfvNV\0"
"glGetVertexAttribIivEXT\0"
"glGetVertexAttribIuivEXT\0"
"glGetVertexAttribiv\0"
"glGetVertexAttribivARB\0"
"glGetVertexAttribivNV\0"
"glGetVertexAttribPointerv\0"
"glGetVertexAttribPointervARB\0"
"glGetVertexAttribPointervNV\0"
"glGlobalAlphaFactorbSUN\0"
"glGlobalAlphaFactordSUN\0"
"glGlobalAlphaFactorfSUN\0"
"glGlobalAlphaFactoriSUN\0"
"glGlobalAlphaFactorsSUN\0"
"glGlobalAlphaFactorubSUN\0"
"glGlobalAlphaFactoruiSUN\0"
"glGlobalAlphaFactorusSUN\0"
"glHint\0"
"glHintPGI\0"
"glHistogram\0"
"glHistogramEXT\0"
"glIglooInterfaceSGIX\0"
"glImageTransformParameterfHP\0"
"glImageTransformParameterfvHP\0"
"glImageTransformParameteriHP\0"
"glImageTransformParameterivHP\0"
"glIndexd\0"
"glIndexdv\0"
"glIndexf\0"
"glIndexFuncEXT\0"
"glIndexfv\0"
"glIndexi\0"
"glIndexiv\0"
"glIndexMask\0"
"glIndexMaterialEXT\0"
"glIndexPointer\0"
"glIndexPointerEXT\0"
"glIndexPointerListIBM\0"
"glIndexs\0"
"glIndexsv\0"
"glIndexub\0"
"glIndexubv\0"
"glInitNames\0"
"glInsertComponentEXT\0"
"glInstrumentsBufferSGIX\0"
"glInterleavedArrays\0"
"glIsArraySetARB\0"
"glIsAsyncMarkerSGIX\0"
"glIsBuffer\0"
"glIsBufferARB\0"
"glIsEnabled\0"
"glIsEnabledIndexedEXT\0"
"glIsFenceAPPLE\0"
"glIsFenceNV\0"
"glIsFramebufferEXT\0"
"glIsList\0"
"glIsObjectBufferATI\0"
"glIsOcclusionQuery\0"
"_glIsOcclusionQueryNV\0"
"glIsOcclusionQueryNV\0"
"glIsProgram\0"
"glIsProgramARB\0"
"glIsProgramNV\0"
"glIsQuery\0"
"glIsQueryARB\0"
"glIsRenderbufferEXT\0"
"glIsShader\0"
"glIsTexture\0"
"glIsTextureEXT\0"
"glIsVariantEnabledEXT\0"
"glIsVertexArrayAPPLE\0"
"glIsVertexAttribEnabledAPPLE\0"
"glLightEnviEXT\0"
"glLightEnviSGIX\0"
"glLightf\0"
"glLightfv\0"
"glLighti\0"
"glLightiv\0"
"glLightModelf\0"
"glLightModelfv\0"
"glLightModeli\0"
"glLightModeliv\0"
"glLineStipple\0"
"glLineWidth\0"
"glLinkProgram\0"
"glLinkProgramARB\0"
"glListBase\0"
"glListParameterfSGIX\0"
"glListParameterfvSGIX\0"
"glListParameteriSGIX\0"
"glListParameterivSGIX\0"
"glLoadIdentity\0"
"glLoadIdentityDeformationMapSGIX\0"
"glLoadMatrixd\0"
"glLoadMatrixf\0"
"glLoadName\0"
"glLoadProgramNV\0"
"glLoadTransposeMatrixd\0"
"glLoadTransposeMatrixdARB\0"
"glLoadTransposeMatrixf\0"
"glLoadTransposeMatrixfARB\0"
"glLockArraysEXT\0"
"glLogicOp\0"
"glMap1d\0"
"glMap1f\0"
"glMap2d\0"
"glMap2f\0"
"glMapBuffer\0"
"glMapBufferARB\0"
"glMapControlPointsNV\0"
"glMapGrid1d\0"
"glMapGrid1f\0"
"glMapGrid2d\0"
"glMapGrid2f\0"
"glMapObjectBufferATI\0"
"glMapParameterfvNV\0"
"glMapParameterivNV\0"
"glMapTexture3DATI\0"
"glMapVertexAttrib1dAPPLE\0"
"glMapVertexAttrib1fAPPLE\0"
"glMapVertexAttrib2dAPPLE\0"
"glMapVertexAttrib2fAPPLE\0"
"glMaterialf\0"
"glMaterialfv\0"
"glMateriali\0"
"glMaterialiv\0"
"glMatrixIndexPointerARB\0"
"glMatrixIndexubvARB\0"
"glMatrixIndexuivARB\0"
"glMatrixIndexusvARB\0"
"glMatrixMode\0"
"glMinmax\0"
"glMinmaxEXT\0"
"glMultiDrawArrays\0"
"glMultiDrawArraysEXT\0"
"glMultiDrawElementArrayAPPLE\0"
"glMultiDrawElements\0"
"glMultiDrawElementsEXT\0"
"glMultiDrawRangeElementArrayAPPLE\0"
"glMultiModeDrawArraysIBM\0"
"glMultiModeDrawElementsIBM\0"
"glMultiTexCoord1d\0"
"glMultiTexCoord1dARB\0"
"glMultiTexCoord1dSGIS\0"
"glMultiTexCoord1dv\0"
"glMultiTexCoord1dvARB\0"
"glMultiTexCoord1dvSGIS\0"
"glMultiTexCoord1f\0"
"glMultiTexCoord1fARB\0"
"glMultiTexCoord1fSGIS\0"
"glMultiTexCoord1fv\0"
"glMultiTexCoord1fvARB\0"
"glMultiTexCoord1fvSGIS\0"
"glMultiTexCoord1hNV\0"
"glMultiTexCoord1hvNV\0"
"glMultiTexCoord1i\0"
"glMultiTexCoord1iARB\0"
"glMultiTexCoord1iSGIS\0"
"glMultiTexCoord1iv\0"
"glMultiTexCoord1ivARB\0"
"glMultiTexCoord1ivSGIS\0"
"glMultiTexCoord1s\0"
"glMultiTexCoord1sARB\0"
"glMultiTexCoord1sSGIS\0"
"glMultiTexCoord1sv\0"
"glMultiTexCoord1svARB\0"
"glMultiTexCoord1svSGIS\0"
"glMultiTexCoord2d\0"
"glMultiTexCoord2dARB\0"
"glMultiTexCoord2dSGIS\0"
"glMultiTexCoord2dv\0"
"glMultiTexCoord2dvARB\0"
"glMultiTexCoord2dvSGIS\0"
"glMultiTexCoord2f\0"
"glMultiTexCoord2fARB\0"
"glMultiTexCoord2fSGIS\0"
"glMultiTexCoord2fv\0"
"glMultiTexCoord2fvARB\0"
"glMultiTexCoord2fvSGIS\0"
"glMultiTexCoord2hNV\0"
"glMultiTexCoord2hvNV\0"
"glMultiTexCoord2i\0"
"glMultiTexCoord2iARB\0"
"glMultiTexCoord2iSGIS\0"
"glMultiTexCoord2iv\0"
"glMultiTexCoord2ivARB\0"
"glMultiTexCoord2ivSGIS\0"
"glMultiTexCoord2s\0"
"glMultiTexCoord2sARB\0"
"glMultiTexCoord2sSGIS\0"
"glMultiTexCoord2sv\0"
"glMultiTexCoord2svARB\0"
"glMultiTexCoord2svSGIS\0"
"glMultiTexCoord3d\0"
"glMultiTexCoord3dARB\0"
"glMultiTexCoord3dSGIS\0"
"glMultiTexCoord3dv\0"
"glMultiTexCoord3dvARB\0"
"glMultiTexCoord3dvSGIS\0"
"glMultiTexCoord3f\0"
"glMultiTexCoord3fARB\0"
"glMultiTexCoord3fSGIS\0"
"glMultiTexCoord3fv\0"
"glMultiTexCoord3fvARB\0"
"glMultiTexCoord3fvSGIS\0"
"glMultiTexCoord3hNV\0"
"glMultiTexCoord3hvNV\0"
"glMultiTexCoord3i\0"
"glMultiTexCoord3iARB\0"
"glMultiTexCoord3iSGIS\0"
"glMultiTexCoord3iv\0"
"glMultiTexCoord3ivARB\0"
"glMultiTexCoord3ivSGIS\0"
"glMultiTexCoord3s\0"
"glMultiTexCoord3sARB\0"
"glMultiTexCoord3sSGIS\0"
"glMultiTexCoord3sv\0"
"glMultiTexCoord3svARB\0"
"glMultiTexCoord3svSGIS\0"
"glMultiTexCoord4d\0"
"glMultiTexCoord4dARB\0"
"glMultiTexCoord4dSGIS\0"
"glMultiTexCoord4dv\0"
"glMultiTexCoord4dvARB\0"
"glMultiTexCoord4dvSGIS\0"
"glMultiTexCoord4f\0"
"glMultiTexCoord4fARB\0"
"glMultiTexCoord4fSGIS\0"
"glMultiTexCoord4fv\0"
"glMultiTexCoord4fvARB\0"
"glMultiTexCoord4fvSGIS\0"
"glMultiTexCoord4hNV\0"
"glMultiTexCoord4hvNV\0"
"glMultiTexCoord4i\0"
"glMultiTexCoord4iARB\0"
"glMultiTexCoord4iSGIS\0"
"glMultiTexCoord4iv\0"
"glMultiTexCoord4ivARB\0"
"glMultiTexCoord4ivSGIS\0"
"glMultiTexCoord4s\0"
"glMultiTexCoord4sARB\0"
"glMultiTexCoord4sSGIS\0"
"glMultiTexCoord4sv\0"
"glMultiTexCoord4svARB\0"
"glMultiTexCoord4svSGIS\0"
"glMultiTexCoordPointerSGIS\0"
"glMultMatrixd\0"
"glMultMatrixf\0"
"glMultTransposeMatrixd\0"
"glMultTransposeMatrixdARB\0"
"glMultTransposeMatrixf\0"
"glMultTransposeMatrixfARB\0"
"glNewBufferRegion\0"
"glNewBufferRegionEXT\0"
"glNewList\0"
"glNewObjectBufferATI\0"
"glNormal3b\0"
"glNormal3bv\0"
"glNormal3d\0"
"glNormal3dv\0"
"glNormal3f\0"
"glNormal3fv\0"
"glNormal3fVertex3fSUN\0"
"glNormal3fVertex3fvSUN\0"
"glNormal3hNV\0"
"glNormal3hvNV\0"
"glNormal3i\0"
"glNormal3iv\0"
"glNormal3s\0"
"glNormal3sv\0"
"glNormalPointer\0"
"glNormalPointerEXT\0"
"glNormalPointerListIBM\0"
"glNormalPointervINTEL\0"
"glNormalStream3bATI\0"
"glNormalStream3bvATI\0"
"glNormalStream3dATI\0"
"glNormalStream3dvATI\0"
"glNormalStream3fATI\0"
"glNormalStream3fvATI\0"
"glNormalStream3iATI\0"
"glNormalStream3ivATI\0"
"glNormalStream3sATI\0"
"glNormalStream3svATI\0"
"glOrtho\0"
"glPassTexCoordATI\0"
"glPassThrough\0"
"glPixelDataRangeNV\0"
"glPixelMapfv\0"
"glPixelMapuiv\0"
"glPixelMapusv\0"
"glPixelStoref\0"
"glPixelStorei\0"
"glPixelTexGenParameterfSGIS\0"
"glPixelTexGenParameterfvSGIS\0"
"glPixelTexGenParameteriSGIS\0"
"glPixelTexGenParameterivSGIS\0"
"glPixelTexGenSGIX\0"
"glPixelTransferf\0"
"glPixelTransferi\0"
"glPixelTransformParameterfEXT\0"
"glPixelTransformParameterfvEXT\0"
"glPixelTransformParameteriEXT\0"
"glPixelTransformParameterivEXT\0"
"glPixelZoom\0"
"glPNTrianglesfATI\0"
"glPNTrianglesiATI\0"
"glPointParameterf\0"
"glPointParameterfARB\0"
"glPointParameterfEXT\0"
"glPointParameterfSGIS\0"
"glPointParameterfv\0"
"glPointParameterfvARB\0"
"glPointParameterfvEXT\0"
"glPointParameterfvSGIS\0"
"glPointParameteri\0"
"glPointParameteriEXT\0"
"glPointParameteriNV\0"
"glPointParameteriv\0"
"glPointParameterivEXT\0"
"glPointParameterivNV\0"
"glPointSize\0"
"glPollAsyncSGIX\0"
"glPollInstrumentsSGIX\0"
"glPolygonMode\0"
"glPolygonOffset\0"
"glPolygonOffsetEXT\0"
"glPolygonStipple\0"
"glPopAttrib\0"
"glPopClientAttrib\0"
"glPopMatrix\0"
"glPopName\0"
"glPrimitiveRestartIndexNV\0"
"glPrimitiveRestartNV\0"
"glPrioritizeTextures\0"
"glPrioritizeTexturesEXT\0"
"glProgramBufferParametersfvNV\0"
"glProgramBufferParametersIivNV\0"
"glProgramBufferParametersIuivNV\0"
"glProgramCallbackMESA\0"
"glProgramEnvParameter4dARB\0"
"glProgramEnvParameter4dvARB\0"
"glProgramEnvParameter4fARB\0"
"glProgramEnvParameter4fvARB\0"
"glProgramEnvParameterI4iNV\0"
"glProgramEnvParameterI4ivNV\0"
"glProgramEnvParameterI4uiNV\0"
"glProgramEnvParameterI4uivNV\0"
"glProgramEnvParameters4fvEXT\0"
"glProgramEnvParametersI4ivNV\0"
"glProgramEnvParametersI4uivNV\0"
"glProgramLocalParameter4dARB\0"
"glProgramLocalParameter4dvARB\0"
"glProgramLocalParameter4fARB\0"
"glProgramLocalParameter4fvARB\0"
"glProgramLocalParameterI4iNV\0"
"glProgramLocalParameterI4ivNV\0"
"glProgramLocalParameterI4uiNV\0"
"glProgramLocalParameterI4uivNV\0"
"glProgramLocalParameters4fvEXT\0"
"glProgramLocalParametersI4ivNV\0"
"glProgramLocalParametersI4uivNV\0"
"glProgramNamedParameter4dNV\0"
"glProgramNamedParameter4dvNV\0"
"glProgramNamedParameter4fNV\0"
"glProgramNamedParameter4fvNV\0"
"glProgramParameter4dNV\0"
"glProgramParameter4dvNV\0"
"glProgramParameter4fNV\0"
"glProgramParameter4fvNV\0"
"glProgramParameteriEXT\0"
"glProgramParameters4dvNV\0"
"glProgramParameters4fvNV\0"
"glProgramStringARB\0"
"glProgramVertexLimitNV\0"
"glPushAttrib\0"
"glPushClientAttrib\0"
"glPushMatrix\0"
"glPushName\0"
"glRasterPos2d\0"
"glRasterPos2dv\0"
"glRasterPos2f\0"
"glRasterPos2fv\0"
"glRasterPos2i\0"
"glRasterPos2iv\0"
"glRasterPos2s\0"
"glRasterPos2sv\0"
"glRasterPos3d\0"
"glRasterPos3dv\0"
"glRasterPos3f\0"
"glRasterPos3fv\0"
"glRasterPos3i\0"
"glRasterPos3iv\0"
"glRasterPos3s\0"
"glRasterPos3sv\0"
"glRasterPos4d\0"
"glRasterPos4dv\0"
"glRasterPos4f\0"
"glRasterPos4fv\0"
"glRasterPos4i\0"
"glRasterPos4iv\0"
"glRasterPos4s\0"
"glRasterPos4sv\0"
"glReadBuffer\0"
"glReadBufferRegion\0"
"glReadBufferRegionEXT\0"
"glReadInstrumentsSGIX\0"
"glReadPixels\0"
"glReadVideoPixelsSUN\0"
"glRectd\0"
"glRectdv\0"
"glRectf\0"
"glRectfv\0"
"glRecti\0"
"glRectiv\0"
"glRects\0"
"glRectsv\0"
"glReferencePlaneSGIX\0"
"glRenderbufferStorageEXT\0"
"glRenderbufferStorageMultisampleCoverageNV\0"
"glRenderbufferStorageMultisampleEXT\0"
"glRenderMode\0"
"glReplacementCodePointerSUN\0"
"glReplacementCodeubSUN\0"
"glReplacementCodeubvSUN\0"
"glReplacementCodeuiColor3fVertex3fSUN\0"
"glReplacementCodeuiColor3fVertex3fvSUN\0"
"glReplacementCodeuiColor4fNormal3fVertex3fSUN\0"
"glReplacementCodeuiColor4fNormal3fVertex3fvSUN\0"
"glReplacementCodeuiColor4ubVertex3fSUN\0"
"glReplacementCodeuiColor4ubVertex3fvSUN\0"
"glReplacementCodeuiNormal3fVertex3fSUN\0"
"glReplacementCodeuiNormal3fVertex3fvSUN\0"
"glReplacementCodeuiSUN\0"
"glReplacementCodeuiTexCoord2fColor4fNormal3fVertex3fSUN\0"
"glReplacementCodeuiTexCoord2fColor4fNormal3fVertex3fvSUN\0"
"glReplacementCodeuiTexCoord2fNormal3fVertex3fSUN\0"
"glReplacementCodeuiTexCoord2fNormal3fVertex3fvSUN\0"
"glReplacementCodeuiTexCoord2fVertex3fSUN\0"
"glReplacementCodeuiTexCoord2fVertex3fvSUN\0"
"glReplacementCodeuiVertex3fSUN\0"
"glReplacementCodeuiVertex3fvSUN\0"
"glReplacementCodeuivSUN\0"
"glReplacementCodeusSUN\0"
"glReplacementCodeusvSUN\0"
"glRequestResidentProgramsNV\0"
"glResetHistogram\0"
"glResetHistogramEXT\0"
"glResetMinmax\0"
"glResetMinmaxEXT\0"
"glResizeBuffersMESA\0"
"glRotated\0"
"glRotatef\0"
"glSampleCoverage\0"
"glSampleCoverageARB\0"
"glSampleMapATI\0"
"glSampleMaskEXT\0"
"glSampleMaskSGIS\0"
"glSamplePassARB\0"
"glSamplePatternEXT\0"
"glSamplePatternSGIS\0"
"glScaled\0"
"glScalef\0"
"glScissor\0"
"glSecondaryColor3b\0"
"glSecondaryColor3bEXT\0"
"glSecondaryColor3bv\0"
"glSecondaryColor3bvEXT\0"
"glSecondaryColor3d\0"
"glSecondaryColor3dEXT\0"
"glSecondaryColor3dv\0"
"glSecondaryColor3dvEXT\0"
"glSecondaryColor3f\0"
"glSecondaryColor3fEXT\0"
"glSecondaryColor3fv\0"
"glSecondaryColor3fvEXT\0"
"glSecondaryColor3hNV\0"
"glSecondaryColor3hvNV\0"
"glSecondaryColor3i\0"
"glSecondaryColor3iEXT\0"
"glSecondaryColor3iv\0"
"glSecondaryColor3ivEXT\0"
"glSecondaryColor3s\0"
"glSecondaryColor3sEXT\0"
"glSecondaryColor3sv\0"
"glSecondaryColor3svEXT\0"
"glSecondaryColor3ub\0"
"glSecondaryColor3ubEXT\0"
"glSecondaryColor3ubv\0"
"glSecondaryColor3ubvEXT\0"
"glSecondaryColor3ui\0"
"glSecondaryColor3uiEXT\0"
"glSecondaryColor3uiv\0"
"glSecondaryColor3uivEXT\0"
"glSecondaryColor3us\0"
"glSecondaryColor3usEXT\0"
"glSecondaryColor3usv\0"
"glSecondaryColor3usvEXT\0"
"glSecondaryColorPointer\0"
"glSecondaryColorPointerEXT\0"
"glSecondaryColorPointerListIBM\0"
"glSelectBuffer\0"
"glSelectTextureCoordSetSGIS\0"
"glSelectTextureSGIS\0"
"glSelectTextureTransformSGIS\0"
"glSeparableFilter2D\0"
"glSeparableFilter2DEXT\0"
"glSetFenceAPPLE\0"
"glSetFenceNV\0"
"glSetFragmentShaderConstantATI\0"
"glSetInvariantEXT\0"
"glSetLocalConstantEXT\0"
"glShadeModel\0"
"glShaderOp1EXT\0"
"glShaderOp2EXT\0"
"glShaderOp3EXT\0"
"glShaderSource\0"
"glShaderSourceARB\0"
"glSharpenTexFuncSGIS\0"
"glSpriteParameterfSGIX\0"
"glSpriteParameterfvSGIX\0"
"glSpriteParameteriSGIX\0"
"glSpriteParameterivSGIX\0"
"glStartInstrumentsSGIX\0"
"glStencilClearTagEXT\0"
"glStencilFunc\0"
"glStencilFuncSeparate\0"
"glStencilFuncSeparateATI\0"
"glStencilMask\0"
"glStencilMaskSeparate\0"
"glStencilOp\0"
"glStencilOpSeparate\0"
"glStencilOpSeparateATI\0"
"glStopInstrumentsSGIX\0"
"glStringMarkerGREMEDY\0"
"glSwapAPPLE\0"
"glSwizzleEXT\0"
"glTagSampleBufferSGIX\0"
"glTangent3bEXT\0"
"glTangent3bvEXT\0"
"glTangent3dEXT\0"
"glTangent3dvEXT\0"
"glTangent3fEXT\0"
"glTangent3fvEXT\0"
"glTangent3iEXT\0"
"glTangent3ivEXT\0"
"glTangent3sEXT\0"
"glTangent3svEXT\0"
"glTangentPointerEXT\0"
"glTbufferMask3DFX\0"
"glTestFenceAPPLE\0"
"glTestFenceNV\0"
"glTestObjectAPPLE\0"
"glTexBufferEXT\0"
"glTexBumpParameterfvATI\0"
"glTexBumpParameterivATI\0"
"glTexCoord1d\0"
"glTexCoord1dv\0"
"glTexCoord1f\0"
"glTexCoord1fv\0"
"glTexCoord1hNV\0"
"glTexCoord1hvNV\0"
"glTexCoord1i\0"
"glTexCoord1iv\0"
"glTexCoord1s\0"
"glTexCoord1sv\0"
"glTexCoord2d\0"
"glTexCoord2dv\0"
"glTexCoord2f\0"
"glTexCoord2fColor3fVertex3fSUN\0"
"glTexCoord2fColor3fVertex3fvSUN\0"
"glTexCoord2fColor4fNormal3fVertex3fSUN\0"
"glTexCoord2fColor4fNormal3fVertex3fvSUN\0"
"glTexCoord2fColor4ubVertex3fSUN\0"
"glTexCoord2fColor4ubVertex3fvSUN\0"
"glTexCoord2fNormal3fVertex3fSUN\0"
"glTexCoord2fNormal3fVertex3fvSUN\0"
"glTexCoord2fv\0"
"glTexCoord2fVertex3fSUN\0"
"glTexCoord2fVertex3fvSUN\0"
"glTexCoord2hNV\0"
"glTexCoord2hvNV\0"
"glTexCoord2i\0"
"glTexCoord2iv\0"
"glTexCoord2s\0"
"glTexCoord2sv\0"
"glTexCoord3d\0"
"glTexCoord3dv\0"
"glTexCoord3f\0"
"glTexCoord3fv\0"
"glTexCoord3hNV\0"
"glTexCoord3hvNV\0"
"glTexCoord3i\0"
"glTexCoord3iv\0"
"glTexCoord3s\0"
"glTexCoord3sv\0"
"glTexCoord4d\0"
"glTexCoord4dv\0"
"glTexCoord4f\0"
"glTexCoord4fColor4fNormal3fVertex4fSUN\0"
"glTexCoord4fColor4fNormal3fVertex4fvSUN\0"
"glTexCoord4fv\0"
"glTexCoord4fVertex4fSUN\0"
"glTexCoord4fVertex4fvSUN\0"
"glTexCoord4hNV\0"
"glTexCoord4hvNV\0"
"glTexCoord4i\0"
"glTexCoord4iv\0"
"glTexCoord4s\0"
"glTexCoord4sv\0"
"glTexCoordPointer\0"
"glTexCoordPointerEXT\0"
"glTexCoordPointerListIBM\0"
"glTexCoordPointervINTEL\0"
"glTexEnvf\0"
"glTexEnvfv\0"
"glTexEnvi\0"
"glTexEnviv\0"
"glTexFilterFuncSGIS\0"
"glTexGend\0"
"glTexGendv\0"
"glTexGenf\0"
"glTexGenfv\0"
"glTexGeni\0"
"glTexGeniv\0"
"glTexImage1D\0"
"glTexImage2D\0"
"glTexImage3D\0"
"glTexImage3DEXT\0"
"glTexImage4DSGIS\0"
"glTexParameterf\0"
"glTexParameterfv\0"
"glTexParameteri\0"
"glTexParameterIivEXT\0"
"glTexParameterIuivEXT\0"
"glTexParameteriv\0"
"glTexScissorFuncINTEL\0"
"glTexScissorINTEL\0"
"glTexSubImage1D\0"
"glTexSubImage1DEXT\0"
"glTexSubImage2D\0"
"glTexSubImage2DEXT\0"
"glTexSubImage3D\0"
"glTexSubImage3DEXT\0"
"glTexSubImage4DSGIS\0"
"glTextureColorMaskSGIS\0"
"glTextureFogSGIX\0"
"glTextureLightEXT\0"
"glTextureMaterialEXT\0"
"glTextureNormalEXT\0"
"glTextureRangeAPPLE\0"
"glTrackMatrixNV\0"
"glTransformFeedbackAttribsNV\0"
"glTransformFeedbackVaryingsNV\0"
"glTranslated\0"
"glTranslatef\0"
"glUniform1f\0"
"glUniform1fARB\0"
"glUniform1fv\0"
"glUniform1fvARB\0"
"glUniform1i\0"
"glUniform1iARB\0"
"glUniform1iv\0"
"glUniform1ivARB\0"
"glUniform1uiEXT\0"
"glUniform1uivEXT\0"
"glUniform2f\0"
"glUniform2fARB\0"
"glUniform2fv\0"
"glUniform2fvARB\0"
"glUniform2i\0"
"glUniform2iARB\0"
"glUniform2iv\0"
"glUniform2ivARB\0"
"glUniform2uiEXT\0"
"glUniform2uivEXT\0"
"glUniform3f\0"
"glUniform3fARB\0"
"glUniform3fv\0"
"glUniform3fvARB\0"
"glUniform3i\0"
"glUniform3iARB\0"
"glUniform3iv\0"
"glUniform3ivARB\0"
"glUniform3uiEXT\0"
"glUniform3uivEXT\0"
"glUniform4f\0"
"glUniform4fARB\0"
"glUniform4fv\0"
"glUniform4fvARB\0"
"glUniform4i\0"
"glUniform4iARB\0"
"glUniform4iv\0"
"glUniform4ivARB\0"
"glUniform4uiEXT\0"
"glUniform4uivEXT\0"
"glUniformBufferEXT\0"
"glUniformMatrix2fv\0"
"glUniformMatrix2fvARB\0"
"glUniformMatrix2x3fv\0"
"glUniformMatrix2x4fv\0"
"glUniformMatrix3fv\0"
"glUniformMatrix3fvARB\0"
"glUniformMatrix3x2fv\0"
"glUniformMatrix3x4fv\0"
"glUniformMatrix4fv\0"
"glUniformMatrix4fvARB\0"
"glUniformMatrix4x2fv\0"
"glUniformMatrix4x3fv\0"
"glUnlockArraysEXT\0"
"glUnmapBuffer\0"
"glUnmapBufferARB\0"
"glUnmapObjectBufferATI\0"
"glUnmapTexture3DATI\0"
"glUpdateObjectBufferATI\0"
"glUseProgram\0"
"glUseProgramObjectARB\0"
"glValidateProgram\0"
"glValidateProgramARB\0"
"glValidBackBufferHintAutodesk\0"
"glVariantArrayObjectATI\0"
"glVariantbvEXT\0"
"glVariantdvEXT\0"
"glVariantfvEXT\0"
"glVariantivEXT\0"
"glVariantPointerEXT\0"
"glVariantsvEXT\0"
"glVariantubvEXT\0"
"glVariantuivEXT\0"
"glVariantusvEXT\0"
"glVertex2d\0"
"glVertex2dv\0"
"glVertex2f\0"
"glVertex2fv\0"
"glVertex2hNV\0"
"glVertex2hvNV\0"
"glVertex2i\0"
"glVertex2iv\0"
"glVertex2s\0"
"glVertex2sv\0"
"glVertex3d\0"
"glVertex3dv\0"
"glVertex3f\0"
"glVertex3fv\0"
"glVertex3hNV\0"
"glVertex3hvNV\0"
"glVertex3i\0"
"glVertex3iv\0"
"glVertex3s\0"
"glVertex3sv\0"
"glVertex4d\0"
"glVertex4dv\0"
"glVertex4f\0"
"glVertex4fv\0"
"glVertex4hNV\0"
"glVertex4hvNV\0"
"glVertex4i\0"
"glVertex4iv\0"
"glVertex4s\0"
"glVertex4sv\0"
"glVertexArrayParameteriAPPLE\0"
"glVertexArrayRangeAPPLE\0"
"glVertexArrayRangeNV\0"
"glVertexAttrib1d\0"
"glVertexAttrib1dARB\0"
"glVertexAttrib1dNV\0"
"glVertexAttrib1dv\0"
"glVertexAttrib1dvARB\0"
"glVertexAttrib1dvNV\0"
"glVertexAttrib1f\0"
"glVertexAttrib1fARB\0"
"glVertexAttrib1fNV\0"
"glVertexAttrib1fv\0"
"glVertexAttrib1fvARB\0"
"glVertexAttrib1fvNV\0"
"glVertexAttrib1hNV\0"
"glVertexAttrib1hvNV\0"
"glVertexAttrib1s\0"
"glVertexAttrib1sARB\0"
"glVertexAttrib1sNV\0"
"glVertexAttrib1sv\0"
"glVertexAttrib1svARB\0"
"glVertexAttrib1svNV\0"
"glVertexAttrib2d\0"
"glVertexAttrib2dARB\0"
"glVertexAttrib2dNV\0"
"glVertexAttrib2dv\0"
"glVertexAttrib2dvARB\0"
"glVertexAttrib2dvNV\0"
"glVertexAttrib2f\0"
"glVertexAttrib2fARB\0"
"glVertexAttrib2fNV\0"
"glVertexAttrib2fv\0"
"glVertexAttrib2fvARB\0"
"glVertexAttrib2fvNV\0"
"glVertexAttrib2hNV\0"
"glVertexAttrib2hvNV\0"
"glVertexAttrib2s\0"
"glVertexAttrib2sARB\0"
"glVertexAttrib2sNV\0"
"glVertexAttrib2sv\0"
"glVertexAttrib2svARB\0"
"glVertexAttrib2svNV\0"
"glVertexAttrib3d\0"
"glVertexAttrib3dARB\0"
"glVertexAttrib3dNV\0"
"glVertexAttrib3dv\0"
"glVertexAttrib3dvARB\0"
"glVertexAttrib3dvNV\0"
"glVertexAttrib3f\0"
"glVertexAttrib3fARB\0"
"glVertexAttrib3fNV\0"
"glVertexAttrib3fv\0"
"glVertexAttrib3fvARB\0"
"glVertexAttrib3fvNV\0"
"glVertexAttrib3hNV\0"
"glVertexAttrib3hvNV\0"
"glVertexAttrib3s\0"
"glVertexAttrib3sARB\0"
"glVertexAttrib3sNV\0"
"glVertexAttrib3sv\0"
"glVertexAttrib3svARB\0"
"glVertexAttrib3svNV\0"
"glVertexAttrib4bv\0"
"glVertexAttrib4bvARB\0"
"glVertexAttrib4d\0"
"glVertexAttrib4dARB\0"
"glVertexAttrib4dNV\0"
"glVertexAttrib4dv\0"
"glVertexAttrib4dvARB\0"
"glVertexAttrib4dvNV\0"
"glVertexAttrib4f\0"
"glVertexAttrib4fARB\0"
"glVertexAttrib4fNV\0"
"glVertexAttrib4fv\0"
"glVertexAttrib4fvARB\0"
"glVertexAttrib4fvNV\0"
"glVertexAttrib4hNV\0"
"glVertexAttrib4hvNV\0"
"glVertexAttrib4iv\0"
"glVertexAttrib4ivARB\0"
"glVertexAttrib4Nbv\0"
"glVertexAttrib4NbvARB\0"
"glVertexAttrib4Niv\0"
"glVertexAttrib4NivARB\0"
"glVertexAttrib4Nsv\0"
"glVertexAttrib4NsvARB\0"
"glVertexAttrib4Nub\0"
"glVertexAttrib4NubARB\0"
"glVertexAttrib4Nubv\0"
"glVertexAttrib4NubvARB\0"
"glVertexAttrib4Nuiv\0"
"glVertexAttrib4NuivARB\0"
"glVertexAttrib4Nusv\0"
"glVertexAttrib4NusvARB\0"
"glVertexAttrib4s\0"
"glVertexAttrib4sARB\0"
"glVertexAttrib4sNV\0"
"glVertexAttrib4sv\0"
"glVertexAttrib4svARB\0"
"glVertexAttrib4svNV\0"
"glVertexAttrib4ubNV\0"
"glVertexAttrib4ubv\0"
"glVertexAttrib4ubvARB\0"
"glVertexAttrib4ubvNV\0"
"glVertexAttrib4uiv\0"
"glVertexAttrib4uivARB\0"
"glVertexAttrib4usv\0"
"glVertexAttrib4usvARB\0"
"glVertexAttribArrayObjectATI\0"
"glVertexAttribI1iEXT\0"
"glVertexAttribI1ivEXT\0"
"glVertexAttribI1uiEXT\0"
"glVertexAttribI1uivEXT\0"
"glVertexAttribI2iEXT\0"
"glVertexAttribI2ivEXT\0"
"glVertexAttribI2uiEXT\0"
"glVertexAttribI2uivEXT\0"
"glVertexAttribI3iEXT\0"
"glVertexAttribI3ivEXT\0"
"glVertexAttribI3uiEXT\0"
"glVertexAttribI3uivEXT\0"
"glVertexAttribI4bvEXT\0"
"glVertexAttribI4iEXT\0"
"glVertexAttribI4ivEXT\0"
"glVertexAttribI4svEXT\0"
"glVertexAttribI4ubvEXT\0"
"glVertexAttribI4uiEXT\0"
"glVertexAttribI4uivEXT\0"
"glVertexAttribI4usvEXT\0"
"glVertexAttribIPointerEXT\0"
"glVertexAttribPointer\0"
"glVertexAttribPointerARB\0"
"glVertexAttribPointerNV\0"
"glVertexAttribs1dvNV\0"
"glVertexAttribs1fvNV\0"
"glVertexAttribs1hvNV\0"
"glVertexAttribs1svNV\0"
"glVertexAttribs2dvNV\0"
"glVertexAttribs2fvNV\0"
"glVertexAttribs2hvNV\0"
"glVertexAttribs2svNV\0"
"glVertexAttribs3dvNV\0"
"glVertexAttribs3fvNV\0"
"glVertexAttribs3hvNV\0"
"glVertexAttribs3svNV\0"
"glVertexAttribs4dvNV\0"
"glVertexAttribs4fvNV\0"
"glVertexAttribs4hvNV\0"
"glVertexAttribs4svNV\0"
"glVertexAttribs4ubvNV\0"
"glVertexBlendARB\0"
"glVertexBlendEnvfATI\0"
"glVertexBlendEnviATI\0"
"glVertexPointer\0"
"glVertexPointerEXT\0"
"glVertexPointerListIBM\0"
"glVertexPointervINTEL\0"
"glVertexStream1dATI\0"
"glVertexStream1dvATI\0"
"glVertexStream1fATI\0"
"glVertexStream1fvATI\0"
"glVertexStream1iATI\0"
"glVertexStream1ivATI\0"
"glVertexStream1sATI\0"
"glVertexStream1svATI\0"
"glVertexStream2dATI\0"
"glVertexStream2dvATI\0"
"glVertexStream2fATI\0"
"glVertexStream2fvATI\0"
"glVertexStream2iATI\0"
"glVertexStream2ivATI\0"
"glVertexStream2sATI\0"
"glVertexStream2svATI\0"
"glVertexStream3dATI\0"
"glVertexStream3dvATI\0"
"glVertexStream3fATI\0"
"glVertexStream3fvATI\0"
"glVertexStream3iATI\0"
"glVertexStream3ivATI\0"
"glVertexStream3sATI\0"
"glVertexStream3svATI\0"
"glVertexStream4dATI\0"
"glVertexStream4dvATI\0"
"glVertexStream4fATI\0"
"glVertexStream4fvATI\0"
"glVertexStream4iATI\0"
"glVertexStream4ivATI\0"
"glVertexStream4sATI\0"
"glVertexStream4svATI\0"
"glVertexWeightfEXT\0"
"glVertexWeightfvEXT\0"
"glVertexWeighthNV\0"
"glVertexWeighthvNV\0"
"glVertexWeightPointerEXT\0"
"glViewport\0"
"glWeightbvARB\0"
"glWeightdvARB\0"
"glWeightfvARB\0"
"glWeightivARB\0"
"glWeightPointerARB\0"
"glWeightsvARB\0"
"glWeightubvARB\0"
"glWeightuivARB\0"
"glWeightusvARB\0"
"glWindowBackBufferHintAutodesk\0"
"glWindowPos2d\0"
"glWindowPos2dARB\0"
"glWindowPos2dMESA\0"
"glWindowPos2dv\0"
"glWindowPos2dvARB\0"
"glWindowPos2dvMESA\0"
"glWindowPos2f\0"
"glWindowPos2fARB\0"
"glWindowPos2fMESA\0"
"glWindowPos2fv\0"
"glWindowPos2fvARB\0"
"glWindowPos2fvMESA\0"
"glWindowPos2i\0"
"glWindowPos2iARB\0"
"glWindowPos2iMESA\0"
"glWindowPos2iv\0"
"glWindowPos2ivARB\0"
"glWindowPos2ivMESA\0"
"glWindowPos2s\0"
"glWindowPos2sARB\0"
"glWindowPos2sMESA\0"
"glWindowPos2sv\0"
"glWindowPos2svARB\0"
"glWindowPos2svMESA\0"
"glWindowPos3d\0"
"glWindowPos3dARB\0"
"glWindowPos3dMESA\0"
"glWindowPos3dv\0"
"glWindowPos3dvARB\0"
"glWindowPos3dvMESA\0"
"glWindowPos3f\0"
"glWindowPos3fARB\0"
"glWindowPos3fMESA\0"
"glWindowPos3fv\0"
"glWindowPos3fvARB\0"
"glWindowPos3fvMESA\0"
"glWindowPos3i\0"
"glWindowPos3iARB\0"
"glWindowPos3iMESA\0"
"glWindowPos3iv\0"
"glWindowPos3ivARB\0"
"glWindowPos3ivMESA\0"
"glWindowPos3s\0"
"glWindowPos3sARB\0"
"glWindowPos3sMESA\0"
"glWindowPos3sv\0"
"glXChooseFBConfig\0"
"glXChooseVisual\0"
"glXCopyContext\0"
"glXCreateContext\0"
"glXCreateGLXPixmap\0"
"glXCreateNewContext\0"
"glXCreatePbuffer\0"
"glXCreatePixmap\0"
"glXCreateWindow\0"
"glXCushionSGI\0"
"glXDestroyContext\0"
"glXDestroyGLXPixmap\0"
"glXDestroyPbuffer\0"
"glXDestroyPixmap\0"
"glXDestroyWindow\0"
"glXGetClientString\0"
"glXGetConfig\0"
"glXGetCurrentContext\0"
"glXGetCurrentDisplay\0"
"glXGetCurrentDrawable\0"
"glXGetCurrentReadDrawable\0"
"glXGetDriverConfig\0"
"glXGetFBConfigAttrib\0"
"glXGetFBConfigs\0"
"glXGetProcAddress\0"
"glXGetScreenDriver\0"
"glXGetSelectedEvent\0"
"glXGetVisualFromFBConfig\0"
"glXIsDirect\0"
"glXMakeContextCurrent\0"
"glXMakeCurrent\0"
"glXQueryContext\0"
"glXQueryDrawable\0"
"glXQueryExtension\0"
"glXQueryExtensionsString\0"
"glXQueryServerString\0"
"glXQueryVersion\0"
"glXSelectEvent\0"
"glXSwapBuffers\0"
"glXUseXFont\0"
"glXWaitGL\0"
"glXWaitX\0"
"\0"
};

static void *wglGetProcAddressFunc(const char *name) {
   /* if ((!strcmp(name, "glXChooseVisual")) ||
        (!strcmp(name, "glXQueryExtensionsString")) ||
        (!strcmp(name, "glXQueryServerString")) ||
        (!strcmp(name, "glXGetClientString")) ||
        (!strcmp(name, "glXCreateContext")) ||
        (!strcmp(name, "glXCreateNewContext")) ||
        (!strcmp(name, "glXCopyContext")) ||
        (!strcmp(name, "glXDestroyContext")) ||
        (!strcmp(name, "glXQueryVersion")) ||
        (!strcmp(name, "glXMakeCurrent")) ||
        (!strcmp(name, "glXSwapBuffers")) ||
        (!strcmp(name, "glXGetConfig")) ||
        (!strcmp(name, "glXQueryExtension")) ||
        (!strcmp(name, "glXChooseFBConfig")) ||
        (!strcmp(name, "glXGetFBConfigs")) ||
        (!strcmp(name, "glXGetFBConfigAttrib")) ||
        (!strcmp(name, "glXQueryContext")) ||
        (!strcmp(name, "glXQueryDrawable")) ||
        (!strcmp(name, "glXGetVisualFromFBConfig")))
        return (void*)1; */// just ANYTHING nonzero will do
    void *res = wglGetProcAddress(name);
    if (res==0) {
        const char *p = KNOWN_GL_FUNCTIONS;
        while (*p) {
            if (!strcmp(name, p))
                return 1;
            // skip to the next '0' and then just over it
            while (*p) p++;
            p++;
        }
    }
    return res;
}
#endif // _WIN32

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
  int formatFlags;
  int i;

  if (attrib_list != NULL) {
    formatFlags = glo_flags_get_from_glx(attrib_list, False);
    for (i=0;i<DIM(FBCONFIGS);i++)
      if (FBCONFIGS[i].formatFlags == formatFlags) {
        if (nelements) *nelements=1;
        return &FBCONFIGS[i];
      }
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
            DEBUGF( "glXGetProcAddress %s  ", (char *) args[0]);
            ret.i = GL_GETPROCADDRESS((const GLubyte *) args[0]) != NULL;
                DEBUGF( " == %08x\n", ret.i);
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
                DEBUGF( "glXGetProcAddress_global %s  ", (char *)huge_buffer);
                result[i] =
                    GL_GETPROCADDRESS((const GLubyte *) huge_buffer) !=
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
