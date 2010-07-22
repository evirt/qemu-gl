#!/bin/bash

TCPIP_MESA_PORT=32432
MESA_GL_PATH=/home/even/opengl/mesa/lib
TCPIP_GL_PATH=/home/even/qemu/cvs280207/qemu/i386-softmmu

function launch_direct_regular
{
  echo "Launching direct with regular GL server : $*...";
  $* 2>/dev/null;
}

function launch_direct_mesa
{
  echo "Launching direct with mesa GL server : $*...";
  LD_LIBRARY_PATH=$MESA_GL_PATH $* 2>/dev/null;
}

function launch_tcpip_regular
{
  echo "Launching through TCP/IP with regular GL server : $*...";
  USE_TCP_COMMUNICATION=1 LD_LIBRARY_PATH=$TCPIP_GL_PATH $* 2>/dev/null;
}

function launch_tcpip_mesa
{
  echo "Launching through TCP/IP with mesa GL server : $*...";
  GL_REMOVE_EXTENSIONS=GL_EXT_shared_texture_palette GL_SERVER_PORT=$TCPIP_MESA_PORT USE_TCP_COMMUNICATION=1 LD_LIBRARY_PATH=$TCPIP_GL_PATH $* 2>/dev/null;
}


function launch_tcpip_mesa_nv
{
  echo "Launching through TCP/IP with mesa NV GL server : $*...";
  GL_REMOVE_EXTENSIONS=GL_ATI,GL_EXT_shared_texture_palette GL_SERVER_PORT=$TCPIP_MESA_PORT USE_TCP_COMMUNICATION=1 LD_LIBRARY_PATH=$TCPIP_GL_PATH $* 2>/dev/null;
}

function launch_tcpip_mesa_ati
{
  echo "Launching through TCP/IP with mesa ATI GL server : $*...";
  GL_REMOVE_EXTENSIONS=GL_NV,GL_EXT_shared_texture_palette GL_SERVER_PORT=$TCPIP_MESA_PORT USE_TCP_COMMUNICATION=1 LD_LIBRARY_PATH=$TCPIP_GL_PATH $* 2>/dev/null;
}

function launch_tcpip_all
{
  launch_direct_regular $*
  launch_direct_mesa $*
  launch_tcpip_regular $*
  launch_tcpip_mesa $*
  launch_tcpip_mesa_nv $*
  launch_tcpip_mesa_ati $*
}

function launch_regular
{
#launch_direct_regular $*;
  launch_tcpip_regular $*
}

function launch_mesa
{
#launch_direct_mesa $*;
  launch_tcpip_mesa $*
}

launch_regular ~/bin/doom3-demo
GL_EXTENSIONS="GL_ARB_multitexture,GL_ARB_texture_env_combine,GL_ARB_texture_cube_map,GL_ARB_texture_env_dot3,GL_ARB_vertex_buffer_object" launch_regular ~/bin/doom3-demo
launch_regular ~/bin/q3demo
GL_EXTENSIONS="" launch_regular ~/bin/q3demo
launch_regular ~/bin/quake4-demo
GL_EXTENSIONS="GL_ARB_texture_compression,GL_EXT_texture_compression_s3tc,GL_ARB_multitexture,GL_ARB_texture_env_combine,GL_ARB_texture_cube_map,GL_ARB_texture_env_dot3,GL_ARB_texture_env_add" launch_regular ~/bin/quake4-demo
launch_regular ~/bin/ut2003demo
GL_EXTENSIONS="GL_EXT_bgra GL_EXT_texture_compression_s3tc" launch_regular ~/bin/ut2003demo
launch_regular ~/bin/ut2004demo
GL_EXTENSIONS="GL_EXT_bgra GL_EXT_texture_compression_s3tc" launch_regular ~/bin/ut2004demo
launch_regular  ~/games/openarena/ioquake3.i386
launch_regular  ~/games/torcs/torcs
cd  ~/games/nexuiz/
launch_regular ./nexuiz-linux-glx
cd  ~/games/nexuiz-2.3/
launch_regular ./nexuiz-linux-glx.sh
cd ~/games/nexuiz-2.2.3/
launch_regular ./nexuiz-linux-glx.sh
cd ~/games/openquartz
launch_regular ./openquartz-glx
launch_regular ./darkplaces/darkplaces-linux-686-glxcontexts
cd ~/games/sauerbraten
launch_regular ./sauerbraten_unix
GL_REMOVE_EXTENSIONS="GL_ARB_occlusion_query" launch_regular ./sauerbraten_unix
cd ~/games/warsow
launch_regular ./warsow

launch_regular /usr/bin/blender

cd ~/WW2D
launch_regular ./ww2d

launch_regular /usr/games/ppracer
launch_regular /usr/games/chromium
launch_regular /usr/games/billard-gl

cd ~/opengl/fgl_glxgears
launch_regular ./fgl_glxgears
launch_regular ./fgl_glxgears -fbo

cd ~/opengl/mesa/progs/tests
launch_regular ./afsmultiarb
launch_regular ./antialias
launch_regular ./arbfpspec
launch_regular ./arbfptest1
launch_regular ./arbfptexture
launch_regular ./arbfptrig
launch_mesa ./arbnpot # with fglrx : "Sorry, this program requires GL_ARB_texture_non_power_of_two"
launch_mesa ./arbnpot-mipmap # with fglrx : "Sorry, this program requires GL_ARB_texture_non_power_of_two"
launch_regular ./arbvptest1
launch_regular ./arbvptest3
launch_regular ./arbvptorus
launch_regular ./arbvpwarpmesh
./arraytexture # requires GL_MESA_texture_array
launch_regular ./blendminmax
launch_regular ./blendsquare
launch_regular ./bufferobj
launch_regular ./bug-3050
launch_regular ./bug-3101
launch_regular ./bug-3195
launch_regular ./copypixrate
launch_regular ./crossbar
launch_regular ./cva
launch_regular ./dinoshade
launch_regular ./drawbuffers # with direct fglrx : ./drawbuffers: symbol lookup error: ./drawbuffers: undefined symbol: glDrawBuffersARB. But works fine throught TCP/IP fglrx :-). Well the test shoudln't really link directly to the glDrawBuffersARB. symbol but rather do a glXGetProcAddressARB on it.
launch_regular ./fbotest1
launch_regular ./fbotest2
launch_regular ./fbotexture # with fglrx : runs but warnings 'Framebuffer incomplete!!!'
launch_mesa ./fbotexture
./floattext # doesn't work neither with fglrx or mesa
launch_regular ./fog
launch_regular ./fogcoord
launch_mesa ./fptest1 # with fglrx : "Sorry, this program requires GL_NV_fragment_program"
launch_mesa ./fptexture # with fglrx : "Error: GL_NV_fragment_program not supported!"
launch_regular ./getprocaddress
launch_regular ./interleave
launch_mesa ./invert  # with fglrx : Sorry, this program requires GL_MESA_pack_invert."
launch_regular ./jkrahntest 1
launch_regular ./jkrahntest 2
# currently KO for TCP/IP
launch_regular ./jkrahntest 3
# currently KO for TCP/IP
launch_regular ./jkrahntest 4
launch_mesa ./jkrahntest 5  # with fglrx :X Error of failed request:  BadMatch (invalid parameter attributes)
launch_regular ./manytex
launch_regular ./mipmap_limits
# Currently KO for TCP/IP because GL_EXT_paletted_texture not implemented
launch_mesa ./multipal # Requires GL_EXT_paletted_texture
# KO because of the optimization that fakes glGetError
launch_regular ./no_s3tc
DISABLE_OPTIM=1 launch_regular ./no_s3tc
launch_regular ./packedpixels
launch_mesa ./pbo # fglrx declares GL_ARB_pixel_buffer_object but not GL_EXT_pixel_buffer_object
launch_mesa ./pbo-even
launch_regular ./prog_parameter
launch_regular ./projtex
launch_regular ./readrate
launch_regular ./seccolor
launch_regular ./sharedtex
# KO because of segfault on end
launch_regular ./stencilwrap
launch_regular ./stencil_wrap
launch_regular ./subtexrate
launch_regular ./tex1d
launch_regular ./texcompress2
launch_regular ./texfilt
launch_regular ./texline
launch_regular ./texobjshare
launch_regular ./texwrap
# Currently KO for TCP/IP because GL_APPLE_vertex_array_object not implemented
launch_mesa ./vao_01 # requires GL_APPLE_vertex_array_object
# Currently KO for TCP/IP because GL_APPLE_vertex_array_object not implemented
launch_mesa ./vao_02 # requires GL_APPLE_vertex_array_object
launch_mesa ./vparray # GL_NV_vertex_program
launch_mesa ./vptest1  # segfaults with regular mesa
launch_mesa ./vptest2  # segfaults with regular mesa
launch_mesa ./vptest3 # GL_NV_vertex_program
launch_mesa ./vptorus # GL_NV_vertex_program
launch_mesa ./vpwarpmesh # GL_NV_vertex_program
launch_mesa ./yuvrect  #GL_NV_texture_rectangle
launch_mesa ./yuvsquare #GL_MESA_ycbcr_texture
launch_regular ./zreaddraw

cd ~/opengl/mesa/progs/xdemos
# currently KO for TCP/IP : not at all MT safe !. KO for direct fglrx too...
launch_mesa ./glthreads
launch_regular ./glxcontexts
# partially OK : problem with window resizing
launch_regular ./glxdemo
launch_regular ./glxgears
launch_regular ./glxgears_fbconfig
# partially OK : only with one X session...
launch_regular ./glxheads
launch_regular ./glxinfo -l
launch_mesa ./glxpbdemo 512 512 foo.ppm #  with fglrx : "Requires GLX 1.3 or 1.4"
# KO for TCP/IP : glXCreateGLXPixmap unsupported
launch_regular ./glxpixmap
# Unable to set swap-interval.  Neither GLX_SGI_swap_control nor GLX_MESA_swap_control are supported.
launch_regular ./glxswapcontrol -swap 30 -forcegetrate
# Unable to set swap-interval.  Neither GLX_SGI_swap_control nor GLX_MESA_swap_control are supported.
launch_mesa ./glxswapcontrol -swap 30 -forcegetrate
# partially OK : sometimes a window remains white
launch_regular ./manywin 5
launch_regular ./offset
launch_mesa ./pbdemo 512 512 foo.ppm #  with fglrx :  "Requires pbuffers"
launch_mesa ./pbinfo # with fglrx : "Error: glxGetFBConfigs failed"
# KO for TCP/IP. Probably something to do with glXMakeContextCurrent
launch_mesa ./wincopy # with fglrx: "Sorry, this program requires either GLX 1.3 or GLX_SGI_make_current_read."
# partially OK : problem with window resizing
launch_regular ./xfont
# partially OK : problem with window resizing
launch_regular ./xrotfont
./yuvrect_client # Don't work neither with fglrx or mesa

cd ~/opengl/mesa/progs/demos
launch_regular ./arbfplight
launch_regular ./arbfslight
launch_regular ./arbocclude
launch_regular ./bounce
launch_regular ./clearspd
launch_regular ./cubemap
launch_regular ./drawpix
launch_regular ./engine
launch_regular ./fire
launch_mesa ./fogcoord   # Requires GL_EXT_fog_coord for complete testing, but can start without the extension
launch_mesa ./fplight # Requires GL_NV_vertex_program
launch_regular ./fslight
launch_regular ./gamma
launch_regular ./gearbox
launch_regular ./gears
launch_regular ./geartrain  # glutInit(&argc, argv); to make it works
launch_regular ./glinfo
launch_regular ./glslnoise
launch_regular ./gltestperf
launch_regular ./glutfx
launch_regular ./ipers
launch_regular ./isosurf    # glutInit(&argc, argv); to make it works
launch_regular ./lodbias
launch_regular ./morph3d
launch_regular ./multiarb
# Currently KO for TCP/IP because GL_EXT_paletted_texture not implemented
launch_mesa ./paltex  # Requires GL_EXT_paletted_texture
launch_regular ./pixel_transfer_test
launch_regular ./pointblast
launch_regular ./ray
launch_regular ./readpix
launch_mesa ./readpix # for GL_OES_read_format support
launch_regular ./reflect
launch_regular ./renormal
launch_regular ./shadowtex
launch_regular ./singlebuffer
launch_regular ./spectex
launch_regular ./spriteblast
launch_regular ./stex3d
launch_regular ./streaming_rect
launch_regular ./teapot
launch_regular ./terrain
launch_regular ./tessdemo
launch_regular ./texcyl
launch_regular ./texdown
launch_regular ./texenv
launch_regular ./texobj
launch_regular ./trispd
launch_regular ./tunnel
launch_regular ./tunnel2
# Currently KO for TCP/IP because GL_APPLE_vertex_array_object not implemented
launch_mesa ./vao_demo # requires GL_APPLE_vertex_array_object
launch_regular ./winpos # glutInit(&argc, argv); to make it works