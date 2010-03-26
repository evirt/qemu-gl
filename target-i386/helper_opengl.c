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

#include "opengl_func.h"
#include "opengl_process.h"


extern void init_process_tab(void);
extern int do_function_call(ProcessStruct *process, int func_number, arg_t *args, char *ret_string);

void do_disconnect(ProcessStruct *process);
#define disconnect(a) \
	do_disconnect(a);

ProcessStruct *do_context_switch(pid_t pid, int switch_gl_context);

static inline int do_decode_call_int(ProcessStruct *process, void *args_in, int args_len, char *r_buffer)
{
    Signature *signature;
    int i, ret;
    char *argptr, *tmp;
    static arg_t args[50];
    int func_number;

    if(!args_len)
	return 0;

    argptr = args_in;

    while((char*)argptr < (char*)args_in + args_len) {
    func_number = *(short*)argptr;
    argptr += 2;

    signature = (Signature *) tab_opengl_calls[func_number];

    tmp = argptr;

    for (i = 0; i < signature->nb_args; i++) {
        int args_size = *(int*)argptr;
	argptr+=4;
        switch (signature->args_type[i]) {
        case TYPE_UNSIGNED_INT:
        case TYPE_INT:
        case TYPE_UNSIGNED_CHAR:
        case TYPE_CHAR:
        case TYPE_UNSIGNED_SHORT:
        case TYPE_SHORT:
        case TYPE_FLOAT:
	    args[i] = *(int*)argptr;
            break;

        case TYPE_NULL_TERMINATED_STRING:
          CASE_IN_UNKNOWN_SIZE_POINTERS:
            if(*(int*)argptr) args[i] = (arg_t)argptr+4; else args[i] = (arg_t)NULL; argptr += 4;
            if (args[i] == 0 && args_size == 0) {
                if (!IS_NULL_POINTER_OK_FOR_FUNC(func_number)) {
                    fprintf(stderr, "call %s arg %d pid=%d\n",
                            tab_opengl_calls_name[func_number], i, process->process_id);
                    return 0;
                }
            } else if (args[i] == 0 && args_size != 0) {
                fprintf(stderr, "call %s arg %d pid=%d\n",
                        tab_opengl_calls_name[func_number], i, process->process_id);
                fprintf(stderr, "A) args[i] == 0 && args_size[i] != 0 !!\n");
                return 0;
            } else if (args[i] != 0 && args_size == 0) {
                fprintf(stderr, "call %s arg %d pid=%d\n",
                        tab_opengl_calls_name[func_number], i, process->process_id);
                fprintf(stderr, "B) args[i] != 0 && args_size[i] == 0 !!\n");
                return 0;
            }
            break;

          CASE_IN_LENGTH_DEPENDING_ON_PREVIOUS_ARGS:
            if(*(int*)argptr) args[i] = (arg_t)argptr+4; else args[i] = (arg_t)NULL; argptr += 4;
            {
//                args_size[i] =
//                    compute_arg_length(stderr, func_number, i, args);
                if (args[i] == 0 && args_size != 0) {
                    fprintf(stderr, "call %s arg %d pid=%d\n",
                            tab_opengl_calls_name[func_number], i, process->process_id);
                    fprintf(stderr, "C) can not get %d bytes\n",
                            args_size);
                    return 0;
                }
                break;
            }

          CASE_OUT_POINTERS:
            {
                if(*(int*)argptr) {
                    *(int*)r_buffer = args_size; r_buffer+=4;
                    args[i] = (arg_t) r_buffer;//(arg_t)argptr+4;
                }
                else
                    args[i] = (arg_t)NULL;
                argptr += 4;
                if (args[i] == 0 && args_size == 0) {
                    if (!IS_NULL_POINTER_OK_FOR_FUNC(func_number)) {
                        fprintf(stderr, "D) all %s arg %d pid=%d\n",
                                tab_opengl_calls_name[func_number], i,
                                process->process_id);
                        return 0;
                    }
                    fprintf(stderr, "E) all %s arg %d pid=%d\n",
                            tab_opengl_calls_name[func_number], i, process->process_id);
                    return 0;
                } else if (args[i] == 0 && args_size != 0) {
                    fprintf(stderr, "call %s arg %d pid=%d\n",
                            tab_opengl_calls_name[func_number], i, process->process_id);
                    fprintf(stderr,
                            "F) args[i] == 0 && args_size[i] != 0 !!\n");
                    return 0;
                } else if (args[i] != 0 && args_size == 0) {
                    fprintf(stderr, "call %s arg %d pid=%d\n",
                            tab_opengl_calls_name[func_number], i, process->process_id);
                    fprintf(stderr,
                            "G) args[i] != 0 && args_size[i] == 0 !!\n");
                    return 0;
                }
            break;
            }   // End of CASE_OUT_POINTERS:

        case TYPE_DOUBLE:
          CASE_IN_KNOWN_SIZE_POINTERS:
            if(*(int*)argptr) args[i] = (arg_t)argptr+4; else args[i] = (arg_t)NULL; argptr += 4;
            if (args[i] == 0 && args_size != 0) {
                fprintf(stderr, "call %s arg %d pid=%d\n",
                        tab_opengl_calls_name[func_number], i, process->process_id);
                fprintf(stderr, "H) can not get %d bytes\n",
                        tab_args_type_length[signature->args_type[i]]);
                return 0;
            }
            break;

        case TYPE_IN_IGNORED_POINTER:
            args[i] = 0;
            break;

        default:
            fprintf(stderr, "shouldn't happen : call %s arg %d pid=%d\n",
                    tab_opengl_calls_name[func_number], i, process->process_id);
            return 0;
        }
	argptr += args_size;
    }

    if (signature->ret_type == TYPE_CONST_CHAR)
        r_buffer[0] = 0;

    ret = do_function_call(process, func_number, args, r_buffer);

    }  // endwhile

    switch(signature->ret_type) {
      case TYPE_INT:
        memcpy(r_buffer, &ret, sizeof(int));
        break;
      case TYPE_CHAR:
        *r_buffer = ret & 0xff;
        break;
    }

    return 1;
}

int decode_call_int(int pid, char *in_args, int args_len,
                                  char *r_buffer)
{
    static ProcessStruct *process = NULL;
    static int first;
    int ret;
    int first_func = *(short*)in_args;

    if(!first) {
        first = 1;
        init_process_tab();
    }

    /* Select the appropriate context for this pid if it isnt already active */
    /* Note: if we're about to execute glXMakeCurrent() then we tell the
             renderer not to waste its time switching contexts */
    if (!process || process->process_id != pid)
	process = do_context_switch(pid,
                                    (first_func == glXMakeCurrent_func)?0:1);

    if(unlikely(first_func == _init32_func || first_func == _init64_func)) {
        if(!process->wordsize) {
            process->wordsize = first_func == _init32_func?4:8;

            *(int*)r_buffer = 2; // Return that we can buffer commands

            return 1; // Initialisation done
        }
        else {
            fprintf(stderr, "Attempt to init twice. Continuing regardless.\n");
        }
    }

    if(unlikely(first_func == -1 || !process->wordsize)) {
        if(!process->wordsize && !first_func != -1)
            fprintf(stderr, "commands submitted before process init.\n");
        ret = 0;
    }
    else {
        ret = do_decode_call_int(process, in_args, args_len, r_buffer);
    }

    if(!ret)
        disconnect(process);

    return ret;
}
