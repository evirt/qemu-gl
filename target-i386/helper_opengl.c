/*
 *  Host-side implementation of GL/GLX API
 *
 *  Copyright (c) 2006,2007 Even Rouault
 *  Modified by: 
 *    Gordon Williams <gordon.williams@collabora.co.uk>
 *    Ian Molton <ian.molton@collabora.co.uk>
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
#include "osdep.h"
#include "opengl_func.h"
#include "opengl_process.h"
#include "opengl_exec.h"

//#include "kvm.h"

#ifdef _WIN32
#define DEBUGF(...) printf(__VA_ARGS__)
#else
extern struct FILE *stderr;		/* Standard error output stream.  */
#define DEBUGF(...) fprintf(stderr, __VA_ARGS__)
#endif

extern int decoding_log_enabled (void);
extern void log_decoding_input (ProcessStruct *process, char *in_args, int args_len);
extern void log_decoding_output (char *r_buffer);

/* do_decode_call_int()
 *
 * Loop through the buffered command stream executing each OpenGL call in
 * sequence. due to the way calls are buffered, only the last call in the
 * buffer may have 'out' parameters or a non-void return code. This allows
 * for efficient buffering whilst avoiding un-necessary buffer flushes.
 */
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

        if(func_number >= GL_N_CALLS) {
            DEBUGF("Bad function number or corrupt command queue\n");
            return 0;
        }

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
                {
                    if(*(int*)argptr)
                        args[i] = (arg_t)argptr+4;
                    else
                        args[i] = (arg_t)NULL;

                    if ((args[i] == 0 && args_size == 0 &&
                        !IS_NULL_POINTER_OK_FOR_FUNC(func_number)) ||
                        (args[i] == 0 && args_size != 0) ||
                        (args[i] != 0 && args_size == 0))
                            return 0;

                    argptr += 4;
                    break;
                }

                CASE_IN_LENGTH_DEPENDING_ON_PREVIOUS_ARGS:
                {
                    if(*(int*)argptr)
                        args[i] = (arg_t)argptr+4;
                    else
                        args[i] = (arg_t)NULL;

                    if (args[i] == 0 && args_size != 0)
                        return 0;

                    argptr += 4;
                    break;
                }

                CASE_OUT_POINTERS:
                {
                    /* It seems that we never experience NULL out pointers!!! */
                    if (args_size == 0 && func_number != 4) // FIXMEIM - hack for now
                        return 0;

                    if(*(int*)argptr) {
                        *(int*)r_buffer = args_size;
                        r_buffer+=4;
                        args[i] = (arg_t)r_buffer;
                        r_buffer += args_size;
                    }
                    else {
                        args[i] = 0;
                    }

                    argptr += 4;
                    args_size = 0;
                    break;
                } 

                case TYPE_DOUBLE:
                CASE_IN_KNOWN_SIZE_POINTERS:
                {
                    if(*(int*)argptr)
                        args[i] = (arg_t)argptr+4;
                    else
                        args[i] = (arg_t)NULL;

                    if (args[i] == 0 && args_size != 0)
                        return 0;

                    argptr += 4;
                    break;
                }

                case TYPE_IN_IGNORED_POINTER:
                    args[i] = 0;
                    break;

                default:
                    DEBUGF( "Oops : call %s arg %d pid=%d\n",
                            tab_opengl_calls_name[func_number], i,
                            process->process_id);
                    return 0;
            }
            argptr += args_size;
        }

        if((char*)argptr > (char*)args_in + args_len) {
            DEBUGF("Client bug: malformed command, killing process\n");
            return 0;
        }

        if (signature->ret_type == TYPE_CONST_CHAR)
            r_buffer[0] = 0; // In case high bits are set.

        ret = do_function_call(process, func_number, args, r_buffer);

    }  // endwhile

    switch(signature->ret_type) {
        case TYPE_INT:
        case TYPE_UNSIGNED_INT:
            memcpy(r_buffer, &ret, sizeof(int));
            break;
        case TYPE_CHAR:
        case TYPE_UNSIGNED_CHAR:
            *r_buffer = ret & 0xff;
            break;
        case TYPE_CONST_CHAR:
            break;
        default:
           DEBUGF("Unsupported GL API return type %i!\n", signature->ret_type);
           exit (-1);
    }

    return 1;
}

#define GLINIT_FAIL_ABI 3
#define GLINIT_QUEUE 2
#define GLINIT_NOQUEUE 1

int decode_call_int(ProcessStruct *process, char *in_args, int args_len, char *r_buffer)
{
    static ProcessStruct *cur_process = NULL;
    int ret;
    int first_func = *(short*)in_args;

    if (decoding_log_enabled ()) {
        log_decoding_input (process, in_args, args_len);
    }
    /* Select the appropriate context for this pid if it isnt already active
     * Note: if we're about to execute glXMakeCurrent() then we tell the
     * renderer not to waste its time switching contexts
     */

    if (cur_process != process) {
        cur_process = process;
        vmgl_context_switch(cur_process, (first_func == glXMakeCurrent_func)?0:1);
    }

    if(unlikely(first_func == _init32_func || first_func == _init64_func)) {
        if(!cur_process->wordsize) {
            int *version = (int*)(in_args+2);
            cur_process->wordsize = first_func == _init32_func?4:8;

            if(version[0] != 1)
                *(int*)r_buffer = GLINIT_FAIL_ABI; // ABI check FAIL
            else if(version[1] != 1)
                *(int*)r_buffer = GLINIT_FAIL_ABI; // ABI check FAIL
            else
                *(int*)r_buffer = GLINIT_QUEUE; // Indicate that we can buffer commands

            return 1; // Initialisation done
        }
        else {
            DEBUGF("Attempt to init twice. Continuing regardless.\n");
            return 1;
        }
    }

    if(unlikely(first_func == -1 || !cur_process->wordsize)) {
        if(!cur_process->wordsize && first_func != -1)
            DEBUGF("commands submitted before process init.\n");
        ret = 0;
    }
    else {
        ret = do_decode_call_int(cur_process, in_args, args_len, r_buffer);
    }

    if(!ret)
        cur_process = NULL;
#if 0
    if (decoding_log_enabled ()) {
        log_decoding_output (ret);
    }
#endif
    return ret;
}
