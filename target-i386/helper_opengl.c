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

#define ENABLE_GL_LOG

//extern FILE *stderr;

extern void init_process_tab(void);
extern int do_function_call(int func_number, arg_t *args, char *ret_string);

static int last_process_id = 0;
static int must_save = 0;
#if 0
#error DEATH!
static /*inline*/ void *get_phys_mem_addr(CPUState *env, target_ulong addr)
{
    int mmu_idx;
    int index;
	int i;

    index = (addr >> TARGET_PAGE_BITS) & (CPU_TLB_SIZE - 1);
    mmu_idx = cpu_mmu_index(env);

//    fprintf(stderr, "addr: %x", addr);    
//    if (unlikely(mmu_idx != MMU_USER_IDX))
//        fprintf(stderr, " - not in userland !!!\n");
//    fprintf(stderr, "\n");    

    if (__builtin_expect
        (env->tlb_table[mmu_idx][index].addr_code !=
         (addr & TARGET_PAGE_MASK), 0)) {
        target_ulong ret = cpu_get_phys_page_debug((CPUState *) env, addr);

        if (ret == -1) {
            fprintf(stderr,
                    "not in phys mem " TARGET_FMT_lx "(" TARGET_FMT_lx " "
                    TARGET_FMT_lx ")\n", addr,
                    env->tlb_table[mmu_idx][index].addr_code,
                    addr & TARGET_PAGE_MASK);
            fprintf(stderr, "cpu_x86_handle_mmu_fault = %d\n",
                    cpu_x86_handle_mmu_fault((CPUState *) env, addr, 0, mmu_idx, 1));
            return NULL;
        } else {
            if (ret + TARGET_PAGE_SIZE <= ram_size) {
                return qemu_get_ram_ptr((ret + (((target_ulong) addr) & (TARGET_PAGE_SIZE - 1))));
            } else {
                fprintf(stderr,
                        "cpu_get_phys_page_debug(env, " TARGET_FMT_lx ") == "
                        TARGET_FMT_lx "\n", addr, ret);
                fprintf(stderr,
                        "ram_size= " TARGET_FMT_lx "\n", ret, (target_ulong) ram_size);

	for(i = 0 ; i < ram_size-10 ; i++) {
		char *ptr = qemu_get_ram_ptr(i);
		if(!strncmp("dizzyb00bs", ptr, 10)) {
			fprintf(stderr, "found boobs at: %lx %lx\n", i, ptr);
			break;
		}
	}

                return qemu_get_ram_ptr(i-128);
            }
        }
    } else
        return (void *) addr + env->tlb_table[mmu_idx][index].addend;
}
#else
#if 0
typedef struct PhysPageDesc {
    /* offset in host memory of the page + io_index in the low bits */
    ram_addr_t phys_offset;
    ram_addr_t region_offset;
} PhysPageDesc;

PhysPageDesc *phys_page_find(target_phys_addr_t index);

static /*inline*/ void *get_phys_mem_addr(CPUState *env, target_ulong vaddr)
{
	target_ulong page;
	target_phys_addr_t phys_page_addr;
	PhysPageDesc *p;
	unsigned long pd;

	page = vaddr & TARGET_PAGE_MASK;
	phys_page_addr = cpu_get_phys_page_debug(env, page);

	if (phys_page_addr == -1) {
		fprintf(stderr, "Ohshit\n");
		return NULL;
	}

	p = phys_page_find(phys_page_addr >> TARGET_PAGE_BITS);

	if(!p) {
		fprintf(stderr, "Ohshit ^2\n");
                return NULL;
        }

	pd = p->phys_offset;

	return qemu_get_ram_ptr((pd & TARGET_PAGE_MASK) + (vaddr & ~TARGET_PAGE_MASK));

}
	
#endif

#endif

#ifndef MIN
#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#endif

enum {
    NOT_MAPPED,
    MAPPED_CONTIGUOUS,
    MAPPED_NOT_CONTIGUOUS
};

#define TARGET_ADDR_LOW_ALIGN(x)  ((target_ulong)(x) & ~(TARGET_PAGE_SIZE - 1))

static int argcpy_target_to_host_1_1(CPUState *env, void *host_addr,
                                     target_ulong target_addr, int nb_args)
{
    int ret;
    ret = cpu_memory_rw_debug(env, target_addr, host_addr, nb_args * sizeof(int), 0);
    return ret?0:1;
}

static int wordsize = 0;
static int (*argcpy_target_to_host) (CPUState *env, void *host_addr,
                                     target_ulong target_addr, int nb_args) =
    argcpy_target_to_host_1_1;

void do_disconnect_current(void);
void do_context_switch(Display *dpy, pid_t pid, int call);

static void disconnect_current(void)
{
    last_process_id = 0;

    return do_disconnect_current();
}

static inline int argcpy_target32_to_host(CPUState *env, void *host_addr,
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

static int host_offset = 0;
static void reset_host_offset()
{
    host_offset = 0;
}

/* Return a host pointer with the content of [target_addr, target_addr + len bytes[ */
/* Do not free or modify */
static const void *get_host_read_pointer(CPUState *env,
                target_ulong target_addr, int len)
{
//	int ret;

//    ret = get_target_mem_state(env, target_addr, len);

//    if (ret == NOT_MAPPED) {
//        return NULL;
//    } else if (ret == MAPPED_CONTIGUOUS) {
//        return get_phys_mem_addr(env, target_addr);
//    } else {
//        static int host_mem_size = 0;
//        static void *host_mem = NULL;
        static void *ret;
//        void *new_mem = NULL;

//        if (host_mem_size < host_offset + len) {
//            host_mem_size = 2 * host_mem_size + host_offset + len;
//            host_mem_size += len;
//            new_mem = realloc(host_mem, host_mem_size);
//		if(!host_mem)
//			host_mem = new_mem;
//		if(host_mem != new_mem){
//			fprintf(stderr, "Fucked, give up\n");
//			exit(1);
//		}
//        }
//        ret = host_mem + host_offset;
	ret = malloc(len);
        if(cpu_memory_rw_debug(env, target_addr, ret, len, 0)) {
		fprintf(stderr, "mapping FAIL\n");
		return NULL;
	}
//        host_offset += len;
        return ret;
//    }
}

int doing_opengl = 0;
static int last_func_number = -1;

#ifdef ENABLE_GL_LOG
static FILE *f = NULL;
static int logger_pid = 0;

#define write_gl_debug_init() do { if (f == NULL) f = fopen("/tmp/debug_gl.bin", "wb"); } while(0)

void write_gl_debug_cmd_int(int my_int)
{
    write_gl_debug_init();
    fwrite(&my_int, sizeof(int), 1, f);
    fflush(f);
}

void write_gl_debug_cmd_short(short my_int)
{
    write_gl_debug_init();
    fwrite(&my_int, sizeof(short), 1, f);
    fflush(f);
}

static void inline write_gl_debug_cmd_buffer_with_size(int size, void *buffer)
{
    write_gl_debug_init();
    fwrite(&size, sizeof(int), 1, f);
    if (size)
        fwrite(buffer, size, 1, f);
}

static void inline write_gl_debug_cmd_buffer_without_size(
                int size, void *buffer)
{
    write_gl_debug_init();
    if (size)
        fwrite(buffer, size, 1, f);
}

void write_gl_debug_end(void)
{
    write_gl_debug_init();
    fclose(f);
    f = NULL;
    logger_pid = 0;
    must_save = 0;
}

static inline int is_logging(int pid)
{
    return must_save && pid == logger_pid;
}
#endif

#include <dlfcn.h>
#include <signal.h>

static void (*anticrash_handler) (void *) = NULL;
static void (*show_stack_from_signal_handler) (int, int, int) = NULL;

void my_anticrash_sigsegv_handler(int signum, siginfo_t *info, void *ptr)
{
    static int counter = 0;

    counter++;

    printf("oops\n");

    /* if (show_stack_from_signal_handler && counter == 1) { struct ucontext* 
     * ctxt = (struct ucontext*)ptr; show_stack_from_signal_handler(10,
     * ctxt->uc_mcontext.gregs[REG_EBP], ctxt->uc_mcontext.gregs[REG_ESP]); } */
    anticrash_handler(ptr);

    counter--;
}

static int decode_call_int(CPUState *env, int func_number, int pid,
                           target_ulong target_ret_string,
                           target_ulong in_args, target_ulong in_args_size)
{
    Signature *signature = (Signature *) tab_opengl_calls[func_number];
    int ret_type = signature->ret_type;
    /* int has_out_parameters = signature->has_out_parameters; */
    int nb_args = signature->nb_args;
    int *args_type = signature->args_type;
    int i;
    int ret;
    int *args_size = NULL;
    target_ulong saved_out_ptr[50];
    static char *ret_string = NULL;
    static arg_t args[50];
    static Display *dpy = NULL;

    if (dpy == NULL) {
        void *handle = dlopen("libanticrash.so", RTLD_LAZY);

        if (handle) {
            anticrash_handler = dlsym(handle, "anticrash_handler");
            if (anticrash_handler) {
                fprintf(stderr, "anticrash handler enabled\n");
                struct sigaction sigsegv_action;
                struct sigaction old_sigsegv_action;

                sigsegv_action.sa_sigaction = my_anticrash_sigsegv_handler;
                sigemptyset(&(sigsegv_action.sa_mask));
                sigsegv_action.sa_flags = SA_SIGINFO | SA_NODEFER;
                sigaction(SIGSEGV, &sigsegv_action, &old_sigsegv_action);
            }
        }
        handle = dlopen("libgetstack.so", RTLD_LAZY);
        if (handle) {
            show_stack_from_signal_handler =
                dlsym(handle, "show_stack_from_signal_handler");
        }

        dpy = XOpenDisplay(NULL);
        init_process_tab();
        ret_string = malloc(32768);
    }

    if (unlikely(last_func_number == _exit_process_func &&
                            func_number == _exit_process_func)) {
        last_func_number = -1;
        return 0;
    }

    if (last_process_id != pid) {
        do_context_switch(dpy, pid, func_number);
        last_process_id = pid;
    }

    if (unlikely(func_number == _exit_process_func))
        last_process_id = 0;

    if (!wordsize) {
        if (func_number == _init32_func || func_number == _init64_func) {
            if (func_number == _init32_func) {
                wordsize = 32;
                argcpy_target_to_host = argcpy_target32_to_host;
            } else {
                wordsize = 64;
                argcpy_target_to_host = argcpy_target64_to_host;
            }
        } else
            fprintf(stderr, "commands submitted before initialisation done\n");
    }

    reset_host_offset();

    if (nb_args) {


	//fprintf(stderr, "call %s pid=%d\n", tab_opengl_calls_name[func_number], pid);
        if (argcpy_target_to_host(env, args, in_args, nb_args) == 0) {
            fprintf(stderr, "call %s pid=%d\n",
                    tab_opengl_calls_name[func_number], pid);
            fprintf(stderr, "cannot get call parameters\n");
            disconnect_current();
            return 0;
        }

////////////////////////////////////////////////////////////////////////////////////////////
        args_size =
            (int *) get_host_read_pointer(env, in_args_size,
                                          sizeof(int) * nb_args); //FIXMEIM - sizeof guest ?
        if (args_size == NULL) {
            fprintf(stderr, "call %s pid=%d\n",
                    tab_opengl_calls_name[func_number], pid);
            fprintf(stderr, "cannot get call parameters size\n");
            disconnect_current();
            return 0;
        }
    }

    if (func_number == _serialized_calls_func) {
        int command_buffer_size = args_size[0];
        const void *command_buffer =
            get_host_read_pointer(env, args[0], command_buffer_size);
        int commmand_buffer_offset = 0;

        args_size = NULL;
#ifdef ENABLE_GL_LOG
        if (is_logging(pid))
            write_gl_debug_cmd_short(_serialized_calls_func);
#endif

        while (commmand_buffer_offset < command_buffer_size) {
            func_number =
                *(short *) (command_buffer + commmand_buffer_offset);
            if (!(func_number >= 0 && func_number < GL_N_CALLS)) {
                fprintf(stderr,
                        "func_number >= 0 && func_number < GL_N_CALLS failed at "
                        "commmand_buffer_offset=%d (command_buffer_size=%d)\n",
                        commmand_buffer_offset, command_buffer_size);
                return 0;
            }
            commmand_buffer_offset += sizeof(short);
#ifdef ENABLE_GL_LOG
            if (is_logging(pid))
                write_gl_debug_cmd_short(func_number);
#endif

            signature = (Signature *) tab_opengl_calls[func_number];
            ret_type = signature->ret_type;
            assert(ret_type == TYPE_NONE);
            nb_args = signature->nb_args;
            args_type = signature->args_type;

            for (i = 0; i < nb_args; i++) {
                switch (args_type[i]) {
                case TYPE_UNSIGNED_INT:
                case TYPE_INT:
                case TYPE_UNSIGNED_CHAR:
                case TYPE_CHAR:
                case TYPE_UNSIGNED_SHORT:
                case TYPE_SHORT:
                case TYPE_FLOAT:
                    {
                        args[i] =
                            *(int *) (command_buffer +
                                      commmand_buffer_offset);
#ifdef ENABLE_GL_LOG
                        if (is_logging(pid))
                            write_gl_debug_cmd_int(args[i]);
#endif
                        commmand_buffer_offset += sizeof(int);
                        break;
                    }

                case TYPE_NULL_TERMINATED_STRING:
                  CASE_IN_UNKNOWN_SIZE_POINTERS:
                    {
                        int arg_size =
                            *(int *) (command_buffer +
                                      commmand_buffer_offset);
                        commmand_buffer_offset += sizeof(int);

                        if (arg_size == 0) {
                            args[i] = 0;
                        } else {
                            args[i] =
                                (long) (command_buffer +
                                        commmand_buffer_offset);
                        }

                        if (args[i] == 0) {
                            if (!IS_NULL_POINTER_OK_FOR_FUNC(func_number)) {
                                fprintf(stderr, "call %s arg %d pid=%d\n",
                                        tab_opengl_calls_name[func_number], i,
                                        pid);
                                disconnect_current();
                                return 0;
                            }
                        } else {
                            if (arg_size == 0) {
                                fprintf(stderr, "call %s arg %d pid=%d\n",
                                        tab_opengl_calls_name[func_number], i,
                                        pid);
                                fprintf(stderr, "args_size[i] == 0 !!\n");
                                disconnect_current();
                                return 0;
                            }
                        }
#ifdef ENABLE_GL_LOG
                        if (is_logging(pid))
                            write_gl_debug_cmd_buffer_with_size(arg_size,
                                                                (void *)
                                                                args[i]);
#endif
                        commmand_buffer_offset += arg_size;

                        break;
                    }

                  CASE_IN_LENGTH_DEPENDING_ON_PREVIOUS_ARGS:
                    {
                        int arg_size =
                            compute_arg_length(stderr, func_number, i, args);
                        args[i] =
                            (arg_size) ? (long) (command_buffer +
                                                 commmand_buffer_offset) : 0;
#ifdef ENABLE_GL_LOG
                        if (is_logging(pid))
                            write_gl_debug_cmd_buffer_without_size(arg_size,
                                                                   (void *)
                                                                   args[i]);
#endif
                        commmand_buffer_offset += arg_size;
                        break;
                    }

                  CASE_OUT_POINTERS:
                    {
                        fprintf(stderr,
                                "shouldn't happen TYPE_OUT_xxxx : call %s arg %d pid=%d\n",
                                tab_opengl_calls_name[func_number], i, pid);
                        disconnect_current();
                        return 0;
                    }

                case TYPE_DOUBLE:
                  CASE_IN_KNOWN_SIZE_POINTERS:
                    args[i] =
                        (long) (command_buffer + commmand_buffer_offset);
#ifdef ENABLE_GL_LOG
                    if (is_logging(pid))
                        write_gl_debug_cmd_buffer_without_size(
                                        tab_args_type_length[args_type[i]],
                                        (void *) args[i]);
#endif
                    commmand_buffer_offset +=
                        tab_args_type_length[args_type[i]];
                    break;

                case TYPE_IN_IGNORED_POINTER:
                    args[i] = 0;
                    break;

                default:
                    fprintf(stderr,
                            "shouldn't happen : call %s arg %d pid=%d\n",
                            tab_opengl_calls_name[func_number], i, pid);
                    disconnect_current();
                    return 0;
                }
            }
            do_function_call(func_number, args, ret_string);
        }

        ret = 0;
    } else {
////////////////////////////////////// One-at-a-time calls here 
#ifdef ENABLE_GL_LOG
        if (is_logging(pid))
            write_gl_debug_cmd_short(func_number);
#endif

        for (i = 0; i < nb_args; i++) {
            switch (args_type[i]) {
            case TYPE_UNSIGNED_INT:
            case TYPE_INT:
            case TYPE_UNSIGNED_CHAR:
            case TYPE_CHAR:
            case TYPE_UNSIGNED_SHORT:
            case TYPE_SHORT:
            case TYPE_FLOAT:
#ifdef ENABLE_GL_LOG
                if (is_logging(pid))
                    write_gl_debug_cmd_int(args[i]);
#endif
                break;

            case TYPE_NULL_TERMINATED_STRING:
              CASE_IN_UNKNOWN_SIZE_POINTERS:
                if (args[i] == 0 && args_size[i] == 0) {
                    if (!IS_NULL_POINTER_OK_FOR_FUNC(func_number)) {
                        fprintf(stderr, "call %s arg %d pid=%d\n",
                                tab_opengl_calls_name[func_number], i, pid);
                        disconnect_current();
                        return 0;
                    }
                } else if (args[i] == 0 && args_size[i] != 0) {
                    fprintf(stderr, "call %s arg %d pid=%d\n",
                            tab_opengl_calls_name[func_number], i, pid);
                    fprintf(stderr, "args[i] == 0 && args_size[i] != 0 !!\n");
                    disconnect_current();
                    return 0;
                } else if (args[i] != 0 && args_size[i] == 0) {
                    fprintf(stderr, "call %s arg %d pid=%d\n",
                            tab_opengl_calls_name[func_number], i, pid);
                    fprintf(stderr, "args[i] != 0 && args_size[i] == 0 !!\n");
                    disconnect_current();
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
                        disconnect_current();
                        return 0;
                    }
                }
#ifdef ENABLE_GL_LOG
                if (is_logging(pid))
                    write_gl_debug_cmd_buffer_with_size(args_size[i],
                                                        (void *) args[i]);
#endif
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
                        disconnect_current();
                        return 0;
                    }
#ifdef ENABLE_GL_LOG
                    if (is_logging(pid))
                        write_gl_debug_cmd_buffer_without_size(args_size[i],
                                        (void *) args[i]);
#endif
                    break;
                }

              CASE_OUT_POINTERS:
                {
//                    int mem_state;

#ifdef ENABLE_GL_LOG
                    if (is_logging(pid))
                        switch (args_type[i]) {
                          CASE_OUT_UNKNOWN_SIZE_POINTERS:
                            write_gl_debug_cmd_int(args_size[i]);
                            break;

                        default:
                            break;
                        }
#endif

                    if (func_number == glXQueryExtension_func && args[i] == 0) {
                        saved_out_ptr[i] = 0;
                        continue;
                    }
                    if (args[i] == 0 && args_size[i] == 0) {
                        if (!IS_NULL_POINTER_OK_FOR_FUNC(func_number)) {
                            fprintf(stderr, "call %s arg %d pid=%d\n",
                                    tab_opengl_calls_name[func_number], i,
                                    pid);
                            disconnect_current();
                            return 0;
                        }
                        fprintf(stderr, "call %s arg %d pid=%d\n",
                                tab_opengl_calls_name[func_number], i, pid);
                        disconnect_current();
                        return 0;
                    } else if (args[i] == 0 && args_size[i] != 0) {
                        fprintf(stderr, "call %s arg %d pid=%d\n",
                                tab_opengl_calls_name[func_number], i, pid);
                        fprintf(stderr,
                                "args[i] == 0 && args_size[i] != 0 !!\n");
                        disconnect_current();
                        return 0;
                    } else if (args[i] != 0 && args_size[i] == 0) {
                        fprintf(stderr, "call %s arg %d pid=%d\n",
                                tab_opengl_calls_name[func_number], i, pid);
                        fprintf(stderr,
                                "args[i] != 0 && args_size[i] == 0 !!\n");
                        disconnect_current();
                        return 0;
                    }
 //FIXME - we should not have to copy here once we get shm working.
                    saved_out_ptr[i] = args[i];
                    if (args[i]) {
			fprintf(stderr, "Alloc_args: %d\n", args_size[i]);
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
                    disconnect_current();
                    return 0;
                }
                args[i] = (arg_t) get_host_read_pointer(env,
                                args[i], tab_args_type_length[args_type[i]]);
                if (args[i] == 0) {
                    fprintf(stderr, "call %s arg %d pid=%d\n",
                            tab_opengl_calls_name[func_number], i, pid);
                    fprintf(stderr, "d) can not get %d bytes\n",
                            tab_args_type_length[args_type[i]]);
                    disconnect_current();
                    return 0;
                }
#ifdef ENABLE_GL_LOG
                if (is_logging(pid))
                    write_gl_debug_cmd_buffer_without_size
                        (tab_args_type_length[args_type[i]],
                         (void *) args[i]);
#endif
                break;

            case TYPE_IN_IGNORED_POINTER:
                args[i] = 0;
                break;

            default:
                fprintf(stderr, "shouldn't happen : call %s arg %d pid=%d\n",
                        tab_opengl_calls_name[func_number], i, pid);
                disconnect_current();
                return 0;
            }
        }

        if (ret_type == TYPE_CONST_CHAR) {
            ret_string[0] = 0;
        }

        if (func_number == _init32_func || func_number == _init64_func) {
            if (func_number == _init32_func) {
                if (wordsize != 32) {
                    fprintf(stderr,
                            "clients with different ABIs not supported\n");
                    exit(-1);
                }
            } else {
                if (wordsize != 64) {
                    fprintf(stderr,
                            "clients with different ABIs not supported\n");
                    exit(-1);
                }
            }

            if (must_save && args[0])
                fprintf(stderr, "error: pid %i is already recording\n",
                                logger_pid);
            else if (args[0]) {
                logger_pid = pid;
                must_save = 1;
            }
            *(int *) args[1] = 1; // FIXME - pass alt. value if we use kvm ?
            ret = 0;
        } else {
            ret = do_function_call(func_number, args, ret_string);         // FIXMEIM --------------------------------------------- call here
        }
#ifdef ENABLE_GL_LOG
        if (is_logging(pid) && func_number == glXGetVisualFromFBConfig_func)
            write_gl_debug_cmd_int(ret);
#endif
        for (i = 0; i < nb_args; i++) {
            switch (args_type[i]) {
              CASE_OUT_POINTERS:
                {
                    if (saved_out_ptr[i]) {
			if (cpu_memory_rw_debug(env, saved_out_ptr[i], (void *) args[i], args_size[i], 1)) {
                            fprintf(stderr, "could not copy out parameters "
                                            "back to user space\n");
                            disconnect_current();
                            return 0;
                        }
			fprintf(stderr, "(un)Alloc_args: %d\n", args_size[i]);
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
                    disconnect_current();
                    return 0;
                }
            }
    }

#ifdef ENABLE_GL_LOG
    if (is_logging(pid) && func_number == _exit_process_func) {
        write_gl_debug_end();
    }
#endif

    return ret;
}

static int decode_call(CPUState *env, int func_number, int pid,
                       target_ulong target_ret_string, target_ulong in_args,
                       target_ulong in_args_size)
{
    if (!(func_number >= 0 && func_number < GL_N_CALLS)) {
        fprintf(stderr,
                "func_number >= 0 && func_number < GL_N_CALLS failed\n");
        return 0;
    }

    return decode_call_int(env, func_number, pid,
                    target_ret_string, in_args, in_args_size);
}

void helper_opengl(void)
{
    doing_opengl = 1;
    env->regs[R_EAX] =
        decode_call(env, env->regs[R_EAX], env->regs[R_EBX], env->regs[R_ECX],
                    env->regs[R_EDX], env->regs[R_ESI]);
    doing_opengl = 0;
}

int virtio_opengl_link(char *glbuffer) {
	int *i = (int*)glbuffer;
        int *v = (int*)((char*)glbuffer + 8);
	int ret;
//	static int lastpid, pid;
//	static char *rbuffer;

	cpu_synchronize_state(env);

//	pid = i[1];
//	if(pid != lastpid || !rbuffer)
//		rbuffer = get_phys_mem_addr(env, v[3]);

	doing_opengl = 1;
//	fprintf(stderr, " Entering call: params: %08x %08x   %08x %08x %08x  %08x\n", i[0], i[1], v[0], v[1], v[2], v[3]);
	ret = decode_call(env, i[0], i[1], v[0], v[1], v[2]);
//	fprintf(stderr, "returned from call: ret = %d\n", ret);
	i[0] = ret;

	if (cpu_memory_rw_debug(env, v[3], (void *) i, 4, 1)) {
		fprintf (stderr, "couldnt write back return code\n");
		disconnect_current();
	}
	doing_opengl = 0;
	
//	lastpid = pid;
	return ret;
}

#endif
