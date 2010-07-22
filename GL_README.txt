This file is an introduction to a patch trying to provide accelerated OpenGL
to QEMU.

NEWS
----
* 2007/07/09 :
  - Add support of TCP/IP for Win32 guest (see 'How to use it ?' and
    'Debugging infrastructure' below for compilation and configuration details)
  - Add support for missing calls : glEnableVariantClientStateEXT,
    glDisableVariantClientStateEXT, glVariantPointerEXT, glGetVariantPointerEXT
  - Improve a bit XGL support.It runs now smoothly but with the HACK_XGL environment
    variable defined (still need to figure out why this hack is necessary)
  - Add command-line documentation for opengl_server and opengl_player.
    Among them : an added option to opengl_server, namely -parent-xid=XXXXX,
    enables you to attach the created OpenGL window as a child of XXXXX
    (can be obtained thanks by xwininfo by a click on the emulator window),
    which gives a satisfactory visual integration. Except the fact that
    Windows cursor (which is emulated) will be invisible, because behind
    the OpenGL window... The Windows trick equivalent to XFixesGetCursorImage
    is still to be found to display a soft cursor upon the OpenGL window.
  - In the head of opengl_client.c, a list of useful environment variables

* 2007/07/02 : many many fixes. OpenGL core API and vendor extensions implementation
  is fairly complete. Most OpenGL games and mesa demos/tests/xdemos are OK.
  Read target-i386/regression_tests for a list of tested programs.
  Still pending, XGL support (works poorly in a very hackish way)


Changes/Improvements since third patch version (10 February 2007)
------------------------------------------------------------------
* Fix bug in VBO handling (enable 'spheres' test from chromium to run)
* Fix bug in serialization of attribList --> fix Quake III Arena Demo startup
    
Changes/Improvements since second patch version (10 February 2007)
------------------------------------------------------------------
* Initial support for Windows as a guest (still GNU/Linux i386 as a host) !
  Guest supported API is WGL/OpenGL (opengl32.dll). Really minimum and hackish implementation,
  but enough to make Windows GoogleEarth run
* Better visual integration : 
     - OpenGL window is now child of QEMU SDL window, so it seems to be
       "naturally" integrated into the virtual machine.
     - Mouse cursor is drawned on top of the GL drawing. (This works only
       if the GL drawing is refreshed in a loop, and with a GNU/Linux guest)
* Better support of NVIDIA chips
* Fix annoying bug in QEMU serialization protocol that caused QEMU crashes when
  serialized buffer was exactly a multiple of target page size (aka. ppracer bug)
* gl(Get)PolygonStipple fix and other OpenGL implementation fixes
* Small improvements to make more Mesa demos run
* License change to MIT license.

Changes/Improvements since first patch version (November 2006)
--------------------------------------------------------------

* More complete implementation of OpenGL enabling to execute real-life programs.
* Communication protocol has been rewritten.
* More KQEMU friendly in terms of performances thanks to buffering of non blocking
  calls. Best performance is now achived with KQEMU enabled, which sounds right,
  doesn'it ?
* Code compiles with less warnings and with -O2 flag (though I still hit a GCC
  bug that makes necessary this ugly my_strlen function pointer in helper_opengl.c)


Architecture summary
--------------------

The patch is composed of three parts :
- a patch for QEMU itself that intercepts any call to 'int $0x99' which is the
  interface I have chosen to dedicate to communication between guest user
  programs and QEMU.
  The modification of QEMU existing code is very slight.
  This part is not very elegant as Fabrice Bellard noticed it. He proposed an
  alternative that sound better, but I certainly lack of time and knowledge
  of the related fields to go on that track.
- an OpenGL 'server' receiving OpenGL calls and executing them with the host GL
  library (preferably with accelerated drivers)
- an OpenGL-compliant (well, aiming to be...) library that must be installed on the
  guest virtual machine.
  This library implements the OpenGL API and send the calls to the QEMU host.


Currently supported platforms
-----------------------------
For the moment, only GNU/Linux i386 as a host, and GNU/Linux i386 or Windows as a guest.
This limitation is just the current state of the patch.
I'm pretty confident that the code can be adapted to support 32/64 bits
combinations, and maybe cross endianesses with more work and probably reduced
performance.

Tested environnements :
- Hosts :
   * Ubuntu Edgy Eft 32 bits with ATI X300 and proprietary drivers (fglrx) - development platform
   * Ubundu Dapper Drake 32 bits with NVIDIA proprietary drivers

- Guests :
    * FC4 32 bits
    * FC5 32 bits
    * Windows XP SP1

How to use it ? (ONLY on GNU/Linux i386 as a host and with only i386-softmmu as target-list)
---------------
 * Apply the patch and recompile QEMU (it applies cleanly on 10 feb 2007 CVS)
 * Lauch QEMU with -enable-gl option
 * For GNU/Linux i386 as a guest :
      * Get the compiled libGL.so from ./i386-softmmu and copy it to the guest OS.
        You may need to make a symlink between libGL.so.1 and libGL.so
      * In the guest OS : LD_LIBRARY_PATH=/path/to/replacement/libGL.so glxgears
 * For Windows as a guest :
      * Compile (I personnaly cross-compile) opengl_client.c as opengl32.dll
      * After building qemu, cd to target-i386
         i586-mingw32msvc-gcc -O2 -Wall opengl_client.c -c -I../i386-softmmu -I. -DBUILD_GL32
         i586-mingw32msvc-dllwrap -o opengl32.dll --def opengl32.def -Wl,-enable-stdcall-fixup opengl_client.o -lws2_32
      * Copy the produced opengl32.dll to the directory of the executable you want to run with QEMU/GL.
        (Note : Windows seems to prevent overwriting of system libraries in \windows\system32 while running)

Debugging infrastructure
------------------------

I found very valuable and time-saving to add the two following small utilities.

* An OpenGL TCP/IP server (opengl_server.c), that has the same
role as QEMU. It works together with the OpenGL client library, launched with
the USE_TCP_COMMUNICATION environment variable defined that replaces the 'int $0x99'
communication by a TCP connection to the server. You can then debug very
easily the pure OpenGL part of the mechanism.
The server accepts two commandline arguments :
  * '-debug' that displays all OpenGL function names that are sent by the
    client
  * '-save' that saves all the data in the /tmp/debug_gl.bin file

For the Windows guest, in the directory of the executable, add a text file called
'opengl_client.txt' with the following lines :
USE_TCP_COMMUNICATION=yes
GL_SERVER=10.0.2.2  #replace it with the IP address of the host seen from the guest

    
* An OpenGL player (opengl_player.c). This one plays a file
recorded either by QEMU host side when the environment variable WRITE_GL is
activated on the guest side, either by opengl_server when it's launched with
the argument '-save'. Very useful to replay a sequence that has made crash
the server.


Known-to-work (and-not-work) programs
----------------------
(Host computer : Athlon 64 3200+, 512 MB RAM, Ubuntu Edgy 32 bits,
                 ATI X300 with ATI proprietary drivers
Virtual computer : Fedora Core 5)

* Many programs in 'Mesa-6.5.1/progs/demos' that runs natively on my computer, that is to say :
    - arbfplight        85 fps QEMU/KQEMU , 824 fps native
    - arbfslight        100 fps QEMU/KQEMU, 900 fps native
    - arbocclude        OK
    - bounce            OK 
    - bufferobj         OK (from there the tests are done on the opengl_server not QEMU)
    - clearspd          OK
    - cubemap           OK
    - drawpix           OK
    - engine            OK
    - fire              OK
    - fogcoord          OK
    - fplight           XFAIL on host computer('Sorry, this demo requires
                                              GL_NV_vertex_program')
    - gamma             OK
    - gearbox           OK
    - gears             OK
    - geartrain         OK
    - glinfo            OK
    - gloss             OK
    - glslnoise         OK
    - gltestperf        OK
    - glutfx            OK (though fullscreen prevents mouse events to be sent...)
    - ipers             OK
    - isosurf           OK
    - loadbias          OK
    - morph3d           OK
    - multiarb          OK
    - paltex            XFAIL on host computer ('Sorry, GL_EXT_paletted_texture
                                                not supported')
    - pointblast        OK
    - ray               OK
    - readpix           OK
    - reflect           OK
    - renormal          OK
    - shadowtex         OK
    - singlebuffer      OK
    - spectex           OK
    - spriteblast       OK
    - stex3d            OK
    - teapot            OK
    - terrain           OK
    - tessdemo          OK
    - texcyl            OK
    - texdown           OK
    - texenv            OK
    - texobj            OK
    - trispd            OK
    - tunnel            OK
    - tunnel2           OK
    - vao_demo          XFAIL on host computer ('Sorry, this program requires
                                                GL_APPLE_vertex_array_object')
    - winpos            OK
    
    (wow, that's the first time I test the whole set of programs. I'm pleased
     to see that some of them work without fix or additions to the code ;-))

* fgl_glxgears with and without -fbo. OK in KQEMU/QEMU with very good performance

Now, funnier stuff :

* ppracer : very good performance in KQEMU/QEMU : 40 FPS
            host computer : 60 FPS
* openquartz-glx : good performance with  with TCP/IP server.
                   Doesn't work in KQEMU/QEMU (even with Mesa rendering)
* darkplaces-linux-686-glx (same game as previous one but different 3D engine)
* googleearth (GNU/Linux and windows guest versions): good performance with TCP/IP server and in KQEMU/QEMU
* ww2d : good performance with TCP/IP server and in KQEMU/QEMU
* earth3d : works. performance not very good even on native hardware. not tried in QEMU
* nasa world wind java SDK : good performance with TCP/IP server, not tried in KQEMU/QEMU
* doom3-demo : good performance with TCP/IP server, a bit too slow to play in KQEMU/QEMU
* quake3-arena-demo : good performance with TCP/IP server, not tried in KQEMU/QEMU
* quake4-demo : good performance with TCP/IP server, not tried in KQEMU/QEMU
* unrealtournamenent3-demo : good performance with TCP/IP server, not tried in KQEMU/QEMU
* unrealtournamenent4-demo : good performance with TCP/IP server, not tried in KQEMU/QEMU
* Mandriva 2007 Live CD : didn't manage to make XGL work, but the drak3d program
                          seemed to believe that there was real 3D hardware.

TODO LIST (almost all long as first time)
---------
- [implement correctly full OpenGL API and vendor extensions] Almost done
- integrate in a better way the window that popups on the guest side with the 
  main qemu window.
- integrate it properly into QEMU build system
- fix how the end of the guest process is detected / enable several gl
  contexts at the same time / enable several guest OS processes to use OpenGL
  at the same time / thread safety of the client library...
- much testing and debugging
- clean the code / code review
- more optimizations to reduce the number of necessary round-trips
- improve the way OpenGL extensions are handled, and do what is necessary to
  only require OpenGL 1.0 symbols for host side linking with the host OpenGL
  library. (we could also all symbols dynamically).
- make it run on x86_64, and allowing any combination guest x86/x86_64 and 
  host x86/x86_64, other archs. That means a complete scan of the code and
  think each time if it's really a int, long, void*, etc etc....
- improve security if possible (preventing malicious guest code from crashing 
  host qemu)
- port it to other UNIX-like OS with X11
- port it to Windows platform (first OpenGL / WGL, then D3D through Wine 
  libs ?)
- (make a patch to Valgrind to make it happy with 'int $0x99' when running on 
   guest side ?)



SECURITY/ROBUSTNESS ISSUES
--------------------------

Security is really a huge challenge. It's really easy for guest code to make
QEMU/OpenGL  crash, most of the time due to implementation bugs of mine of course,
but also because of some "features" (to be polite) of some host OpenGL drivers
themselves... During my repeated tests, I have even managed to make my X server
crash from time to time.
I don't really see how we can ensure that QEMU won't crash even when we'll have
corrected most bugs on our side. One way is certainly to execute OpenGL calls
in an external process, like the opengl_server. But in that case, how to do the
integration in QEMU window ?


FILE LIST
---------

target-i386/
 - opengl_client.c, opengl_client_xfonts.c  : the OpenGL guest library
 - helper_opengl.c : decoding of OpenGL calls in QEMU
 - opengl_exec.c : execution of OpenGL calls on host/server side
 - gl_func.h, server_stub.c, client_stub.c : files generated by parse_gl_h.c 
     from gl.h parsing
 - glgetv_cst.h  : file generated by parse_mesa_get_c.c
 - gl_func_perso.h, opengl_func.h : hand-written "prototypes"
 - opengl_player.c : see above
 - opengl_server.c : see above
 - mesa_gl.h, mesa_gl_ext.h, mesa_get.c, mesa_enums.c :
            directly taken from MESA project and just renamed with mesa_ prefix.
            Needed by parse_mesa_get_c

CONTRIBUTING
------------

I hope I've not discouraged people of good will to contribute to. The TODO list
is certainly a good start.

(Side note, it may be interesting if someone could provide some space to host
pre-build patched QEMU binaries and client OpenGL libraries, so that people that want
to try the patch do not need to apply it and rebuild QEMU)

