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
#define _XOPEN_SOURCE 600
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <stdio.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include "exec.h"
#include "kvm.h"

#if defined(CONFIG_USER_ONLY)
void helper_opengl(void)
{
    /* TODO */
}
#else

#include "opengl_func.h"
#include "opengl_process.h"


extern void init_process_tab(void);
extern int do_function_call(ProcessStruct *process, int func_number, arg_t *args, char *ret_string);

void do_disconnect(ProcessStruct *process);
#define disconnect(a) \
	do_disconnect(a);\
        kill_process = 1;\
	a = NULL;

ProcessStruct *do_context_switch(pid_t pid, int switch_gl_context);

int kill_process;

static int argcpy_target32_to_host(CPUState *env, void *host_addr,
                                   target_ulong target_addr, int nb_args)
{
    uint32_t args_temp[nb_args], *src = args_temp;
    arg_t *dest = host_addr;

    if(cpu_memory_rw_debug(env, target_addr, (void*)args_temp, nb_args * 4, 0))
	return 0;

    while (nb_args) {
        /* TODO: endianness */
        *dest++ = *src++;
        nb_args--;
    }

    return 1;
}

static int argcpy_target64_to_host(CPUState *env, void *host_addr,
                                   target_ulong target_addr, int nb_args)
{
    uint64_t args_temp[nb_args], *src = args_temp;
    arg_t *dest = host_addr;

    if(cpu_memory_rw_debug(env, target_addr, (void *)args_temp, nb_args *8, 0))
        return 0;

    while (nb_args) {
        /* TODO: endianness */
        *dest++ = *src++;
        nb_args--;
    }

    return 1;
}

/* Return a host pointer with the content of [target_addr, target_addr + len bytes[ */
/* Do not free or modify */
static const void *get_host_read_pointer(CPUState *env,
                target_ulong target_addr, int len)
{
	// FIXMEIM - HUGE memory leak here
	void *ret = malloc(len);
        if(cpu_memory_rw_debug(env, target_addr, ret, len, 0)) {
		fprintf(stderr, "mapping FAIL\n");
		return NULL;
	}
        return ret;
}

int doing_opengl = 0;

#include <dlfcn.h>
#include <signal.h>

static inline int decode_call_int(CPUState *env, int func_number, int pid,
                           target_ulong target_ret_string,
                           target_ulong in_args, target_ulong in_args_size)
{
    int ret_type, nb_args;
    int *args_type;
    int i, ret;
    int *args_size = NULL;
    target_ulong saved_out_ptr[50];
    static char *ret_string = NULL;
    static arg_t args[50];
    static ProcessStruct *process = NULL;
    static int first;

    if(func_number < -1 || func_number > GL_N_CALLS)
	return 0;

    if(!first) {
        first = 1;
        init_process_tab();
        ret_string = malloc(32768);
    }

    /* Select the appropriate context for this pid if it isnt already active */
    /* Note: if we're about to execute glXMakeCurrent() then we tell the
             renderer not to waste its time switching contexts */
    if (!process || process->process_id != pid)
	process = do_context_switch(pid,
                                    (func_number == glXMakeCurrent_func)?0:1);

    if(unlikely(func_number == _init32_func ||
                func_number == _init64_func)) {
        if(!process->argcpy_target_to_host) {
            if (func_number == _init32_func)
                process->argcpy_target_to_host = argcpy_target32_to_host;
            else
                process->argcpy_target_to_host = argcpy_target64_to_host;
		return 1; // Initialisation OK
        }
        else {
            fprintf(stderr, "Attempt to init twice. Continuing regardless.\n");
        }
    }

    if(unlikely(func_number == -1 || !process->argcpy_target_to_host)) {
        if(!process->argcpy_target_to_host && !func_number != -1)
            fprintf(stderr, "commands submitted before process init.\n");
        disconnect(process);
        return 0;
    }

    do {
        Signature *signature = (Signature *) tab_opengl_calls[func_number];
        ret_type  = signature->ret_type;
        nb_args   = signature->nb_args;
        args_type = signature->args_type;
    } while(0);

    if (nb_args) {
        if (process->argcpy_target_to_host(env, args, in_args, nb_args) == 0) {
            fprintf(stderr, "call %s pid=%d\n",
                    tab_opengl_calls_name[func_number], pid);
            fprintf(stderr, "cannot get call parameters\n");
            disconnect(process);
            return 0;
        }

        ////////////////////////////////////////////////////////////////////////
        args_size =
            (int *) get_host_read_pointer(env, in_args_size,
                                          sizeof(int) * nb_args); //FIXMEIM - sizeof guest ?
        if (args_size == NULL) {
            fprintf(stderr, "call %s pid=%d\n",
                    tab_opengl_calls_name[func_number], pid);
            fprintf(stderr, "cannot get call parameters size\n");
            disconnect(process);
            return 0;
        }
    }

////////////////////////////////////// One-at-a-time calls here 
    do {
        for (i = 0; i < nb_args; i++) {
            switch (args_type[i]) {
            case TYPE_UNSIGNED_INT:
            case TYPE_INT:
            case TYPE_UNSIGNED_CHAR:
            case TYPE_CHAR:
            case TYPE_UNSIGNED_SHORT:
            case TYPE_SHORT:
            case TYPE_FLOAT:
                break;

            case TYPE_NULL_TERMINATED_STRING:
              CASE_IN_UNKNOWN_SIZE_POINTERS:
                if (args[i] == 0 && args_size[i] == 0) {
                    if (!IS_NULL_POINTER_OK_FOR_FUNC(func_number)) {
                        fprintf(stderr, "call %s arg %d pid=%d\n",
                                tab_opengl_calls_name[func_number], i, pid);
                        disconnect(process);
                        return 0;
                    }
                } else if (args[i] == 0 && args_size[i] != 0) {
                    fprintf(stderr, "call %s arg %d pid=%d\n",
                            tab_opengl_calls_name[func_number], i, pid);
                    fprintf(stderr, "args[i] == 0 && args_size[i] != 0 !!\n");
                    disconnect(process);
                    return 0;
                } else if (args[i] != 0 && args_size[i] == 0) {
                    fprintf(stderr, "call %s arg %d pid=%d\n",
                            tab_opengl_calls_name[func_number], i, pid);
                    fprintf(stderr, "args[i] != 0 && args_size[i] == 0 !!\n");
                    disconnect(process);
                    return 0;
                }
                if (args[i]) {
                    args[i] =
                        (arg_t) get_host_read_pointer(env, args[i],
                                                      args_size[i]);
                    if (args[i] == 0) {
                        fprintf(stderr, "call %s arg %d pid=%d\n",
                                tab_opengl_calls_name[func_number], i, pid);
                        fprintf(stderr, "a) can not get %d bytes\n",
                                args_size[i]);
                        disconnect(process);
                        return 0;
                    }
                }
                break;

              CASE_IN_LENGTH_DEPENDING_ON_PREVIOUS_ARGS:
                {
                    args_size[i] =
                        compute_arg_length(stderr, func_number, i, args);
                    args[i] = (args_size[i]) ? (arg_t)
                            get_host_read_pointer(env,
                                            args[i], args_size [i]) : 0;
                    if (args[i] == 0 && args_size[i] != 0) {
                        fprintf(stderr, "call %s arg %d pid=%d\n",
                                tab_opengl_calls_name[func_number], i, pid);
                        fprintf(stderr, "b) can not get %d bytes\n",
                                args_size[i]);
                        disconnect(process);
                        return 0;
                    }
                    break;
                }

              CASE_OUT_POINTERS:
                {
//                    int mem_state;

                    if (func_number == glXQueryExtension_func && args[i] == 0) {
                        saved_out_ptr[i] = 0;
                        continue;
                    }
                    if (args[i] == 0 && args_size[i] == 0) {
                        if (!IS_NULL_POINTER_OK_FOR_FUNC(func_number)) {
                            fprintf(stderr, "call %s arg %d pid=%d\n",
                                    tab_opengl_calls_name[func_number], i,
                                    pid);
                            disconnect(process);
                            return 0;
                        }
                        fprintf(stderr, "call %s arg %d pid=%d\n",
                                tab_opengl_calls_name[func_number], i, pid);
                        disconnect(process);
                        return 0;
                    } else if (args[i] == 0 && args_size[i] != 0) {
                        fprintf(stderr, "call %s arg %d pid=%d\n",
                                tab_opengl_calls_name[func_number], i, pid);
                        fprintf(stderr,
                                "args[i] == 0 && args_size[i] != 0 !!\n");
                        disconnect(process);
                        return 0;
                    } else if (args[i] != 0 && args_size[i] == 0) {
                        fprintf(stderr, "call %s arg %d pid=%d\n",
                                tab_opengl_calls_name[func_number], i, pid);
                        fprintf(stderr,
                                "args[i] != 0 && args_size[i] == 0 !!\n");
                        disconnect(process);
                        return 0;
                    }
 //FIXME - we should not have to copy here once we get shm working.
                    saved_out_ptr[i] = args[i];
                    if (args[i]) {
//			fprintf(stderr, "Alloc_args: %d\n", args_size[i]);
                        args[i] = (arg_t) malloc(args_size[i]);
                    }
                    break;
                }   // End of CASE_OUT_POINTERS:
//FIXMEIM - break?

            case TYPE_DOUBLE:
              CASE_IN_KNOWN_SIZE_POINTERS:
                if (args[i] == 0) {
                    fprintf(stderr, "call %s arg %d pid=%d\n",
                            tab_opengl_calls_name[func_number], i, pid);
                    fprintf(stderr, "c) can not get %d bytes\n",
                            tab_args_type_length[args_type[i]]);
                    disconnect(process);
                    return 0;
                }
                args[i] = (arg_t) get_host_read_pointer(env,
                                args[i], tab_args_type_length[args_type[i]]);
                if (args[i] == 0) {
                    fprintf(stderr, "call %s arg %d pid=%d\n",
                            tab_opengl_calls_name[func_number], i, pid);
                    fprintf(stderr, "d) can not get %d bytes\n",
                            tab_args_type_length[args_type[i]]);
                    disconnect(process);
                    return 0;
                }
                break;

            case TYPE_IN_IGNORED_POINTER:
                args[i] = 0;
                break;

            default:
                fprintf(stderr, "shouldn't happen : call %s arg %d pid=%d\n",
                        tab_opengl_calls_name[func_number], i, pid);
                disconnect(process);
                return 0;
            }
        }

        if (ret_type == TYPE_CONST_CHAR) {
            ret_string[0] = 0;
        }

        ret = do_function_call(process, func_number, args, ret_string);

        for (i = 0; i < nb_args; i++) {
            switch (args_type[i]) {
              CASE_OUT_POINTERS:
                {
                    if (saved_out_ptr[i]) {
			if (cpu_memory_rw_debug(env, saved_out_ptr[i], (void *) args[i], args_size[i], 1)) {
                            fprintf(stderr, "could not copy out parameters "
                                            "back to user space\n");
                            disconnect(process);
                            return 0;
                        }
//			fprintf(stderr, "(un)Alloc_args: %d\n", args_size[i]);
                        free((void *) args[i]);
                    }
                    break;
                }

            default:
                break;
            }
        }

        if (ret_type == TYPE_CONST_CHAR)
            if (target_ret_string) {
		if (cpu_memory_rw_debug(env, target_ret_string, (void *)ret_string, strlen(ret_string) + 1, 1)) {
                    fprintf(stderr, "cannot copy out parameters "
                                    "back to user space\n");
                    disconnect(process);
                    return 0;
                }
            }
    } while (0);

    return ret;
}

int virtio_opengl_link(char *glbuffer) {
    int *i = (int*)glbuffer;

    cpu_synchronize_state(env);

    doing_opengl = 1;

    kill_process = 0;

    i[0] = decode_call_int(env, (int)i[0],           /* func no */
                                (int)i[1],           /* pid*/
                                (target_ulong)i[2],  /* target_ret_string */
                                (target_ulong)i[3],  /* in_args */
                                (target_ulong)i[4]); /* in_args_size */

    if(kill_process)
        i[6] = 0xdeadbeef;

    if (cpu_memory_rw_debug(env, i[5], (void *) i, 7*4, 1)) {
        fprintf (stderr, "couldnt write back return code\n");
    }
    doing_opengl = 0;
	
    return i[0];
}

#endif
