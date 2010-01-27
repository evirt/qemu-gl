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

extern void sdl_set_opengl_window(int x, int y, int width, int height);

static int last_process_id = 0;
static int must_save = 0;

static /*inline*/ void *get_phys_mem_addr(CPUState *env, target_ulong addr)
{
    int mmu_idx;
    int index;

    index = (addr >> TARGET_PAGE_BITS) & (CPU_TLB_SIZE - 1);
    mmu_idx = cpu_mmu_index(env);
    
//    /* A sanity check for the int0x99 case */
//    if (unlikely(mmu_idx != MMU_USER_IDX)) {
//        fprintf(stderr, "not in userland !!!\n");
//        return NULL;
//    }
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
                        "ret=" TARGET_FMT_lx " ram_size= " TARGET_FMT_lx
                        "\n", ret, (target_ulong) ram_size);
                return NULL;
            }
        }
    } else
        return (void *) addr + env->tlb_table[mmu_idx][index].addend;
}

#ifndef MIN
#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#endif

enum {
    NOT_MAPPED,
    MAPPED_CONTIGUOUS,
    MAPPED_NOT_CONTIGUOUS
};

#define TARGET_ADDR_LOW_ALIGN(x)  ((target_ulong)(x) & ~(TARGET_PAGE_SIZE - 1))

/* Return NOT_MAPPED if a page is not mapped into target physical memory */
/* MAPPED_CONTIGUOUS if all pages are mapped into target physical memory and contiguous */
/* MAPPED_NOT_CONTIGUOUS if all pages are mapped into target physical memory but not contiguous */
static int get_target_mem_state(CPUState *env,
                                target_ulong target_addr, int len)
{
    target_ulong aligned_target_addr = TARGET_ADDR_LOW_ALIGN(target_addr);
    int to_end_page =
        (long) aligned_target_addr + TARGET_PAGE_SIZE - (long) target_addr;
    int ret = MAPPED_CONTIGUOUS;

    if (aligned_target_addr != target_addr) {
        void *phys_addr = get_phys_mem_addr(env, aligned_target_addr);
        void *last_phys_addr = phys_addr;

        if (phys_addr == 0) {
            return NOT_MAPPED;
        }
        if (len > to_end_page) {
            len -= to_end_page;
            aligned_target_addr += TARGET_PAGE_SIZE;
            int i;

            for (i = 0; i < len; i += TARGET_PAGE_SIZE) {
                void *phys_addr =
                    get_phys_mem_addr(env, aligned_target_addr + i);
                if (phys_addr == 0) {
                    return NOT_MAPPED;
                }
                if (phys_addr != last_phys_addr + TARGET_PAGE_SIZE)
                    ret = MAPPED_NOT_CONTIGUOUS;
                last_phys_addr = phys_addr;
            }
        }
    } else {
        void *last_phys_addr = NULL;
        int i;

        for (i = 0; i < len; i += TARGET_PAGE_SIZE) {
            void *phys_addr = get_phys_mem_addr(env, target_addr + i);

            if (phys_addr == 0) {
                return NOT_MAPPED;
            }
            if (i != 0 && phys_addr != last_phys_addr + TARGET_PAGE_SIZE)
                ret = MAPPED_NOT_CONTIGUOUS;
            last_phys_addr = phys_addr;
        }
    }
    return ret;
}

/* copy len bytes from host memory at addr host_addr to target memory at logical addr target_addr */
/* Returns 1 if successfull, 0 if some target pages are not mapped into target physical memory */
static int memcpy_host_to_target(CPUState *env,
                                 target_ulong target_addr,
                                 const void *host_addr, int len)
{
    int i;
    target_ulong aligned_target_addr = TARGET_ADDR_LOW_ALIGN(target_addr);
    int to_end_page =
        (long) aligned_target_addr + TARGET_PAGE_SIZE - (long) target_addr;
    int ret = get_target_mem_state(env, target_addr, len);

    if (ret == NOT_MAPPED) {
        return 0;
    }

    if (ret == MAPPED_CONTIGUOUS) {
        void *phys_addr = get_phys_mem_addr(env, target_addr);

        memcpy(phys_addr, host_addr, len);
    } else {
        if (aligned_target_addr != target_addr) {
            void *phys_addr = get_phys_mem_addr(env, target_addr);

            memcpy(phys_addr, host_addr, MIN(len, to_end_page));
            if (len <= to_end_page) {
                return 1;
            }
            len -= to_end_page;
            host_addr += to_end_page;
            target_addr = aligned_target_addr + TARGET_PAGE_SIZE;
        }
        for (i = 0; i < len; i += TARGET_PAGE_SIZE) {
            void *phys_addr = get_phys_mem_addr(env, target_addr + i);

            memcpy(phys_addr, host_addr + i,
                   (i + TARGET_PAGE_SIZE <=
                    len) ? TARGET_PAGE_SIZE : len & (TARGET_PAGE_SIZE - 1));
        }
    }

    return 1;
}

static int memcpy_target_to_host(CPUState *env, void *host_addr,
                                 target_ulong target_addr, int len)
{
    int i;
    target_ulong aligned_target_addr = TARGET_ADDR_LOW_ALIGN(target_addr);
    int to_end_page =
        (long) aligned_target_addr + TARGET_PAGE_SIZE - (long) target_addr;
    int ret = get_target_mem_state(env, target_addr, len);

    if (ret == NOT_MAPPED) {
        return 0;
    }

    if (ret == MAPPED_CONTIGUOUS) {
        void *phys_addr = get_phys_mem_addr(env, target_addr);

        memcpy(host_addr, phys_addr, len);
    } else {
        if (aligned_target_addr != target_addr) {
            void *phys_addr = get_phys_mem_addr(env, target_addr);

            memcpy(host_addr, phys_addr, MIN(len, to_end_page));
            if (len <= to_end_page)
                return 1;

            len -= to_end_page;
            host_addr += to_end_page;
            target_addr = aligned_target_addr + TARGET_PAGE_SIZE;
        }
        for (i = 0; i < len; i += TARGET_PAGE_SIZE) {
            void *phys_addr = get_phys_mem_addr(env, target_addr + i);

            memcpy(host_addr + i, phys_addr,
                   (i + TARGET_PAGE_SIZE <=
                    len) ? TARGET_PAGE_SIZE : len & (TARGET_PAGE_SIZE - 1));
        }
    }

    return 1;
}

static int memcpy_target_to_host_1_1(CPUState *env, void *host_addr,
                                     target_ulong target_addr, int nb_args)
{
    return memcpy_target_to_host(env, host_addr, target_addr, nb_args * 8);
}

static int wordsize = 0;
static int (*argcpy_target_to_host) (CPUState *env, void *host_addr,
                                     target_ulong target_addr, int nb_args) =
    memcpy_target_to_host_1_1;

void do_disconnect_current(void);
void do_context_switch(Display *dpy, pid_t pid, int call);

static void disconnect_current(void)
{
    last_process_id = 0;

    return do_disconnect_current();
}

static int memcpy_target32_to_host(CPUState *env, void *host_addr,
                                   target_ulong target_addr, int nb_args)
{
    int ret;
    uint32_t args_temp[nb_args], *src = args_temp;
    arg_t *dest = host_addr;

    ret = memcpy_target_to_host(env, args_temp, target_addr, nb_args * 4);
    if (!ret)
        return ret;

    while (nb_args) {
        /* TODO: endianness */
        *dest = 0;
        *(uint32_t *) (dest++) = *src++;
        nb_args--;
    }

    return ret;
}

static int memcpy_target64_to_host(CPUState *env, void *host_addr,
                                   target_ulong target_addr, int nb_args)
{
    int ret;
    uint64_t args_temp[nb_args], *src = args_temp;
    arg_t *dest = host_addr;

    ret = memcpy_target_to_host(env, args_temp, target_addr, nb_args * 8);
    if (!ret)
        return ret;

    while (nb_args) {
        /* TODO: endianness */
        *dest = 0;
        *(uint64_t *) (dest++) = *src++;
        nb_args--;
    }

    return ret;
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
	int ret;

    ret = get_target_mem_state(env, target_addr, len);

    if (ret == NOT_MAPPED) {
        return NULL;
    } else if (ret == MAPPED_CONTIGUOUS) {
        return get_phys_mem_addr(env, target_addr);
    } else {
        static int host_mem_size = 0;
        static void *host_mem = NULL;
        static void *ret;

        if (host_mem_size < host_offset + len) {
            host_mem_size = 2 * host_mem_size + host_offset + len;
            host_mem = realloc(host_mem, host_mem_size);
        }
        ret = host_mem + host_offset;
        assert(memcpy_target_to_host(env, ret, target_addr, len));
        host_offset += len;
        return ret;
    }
}

int doing_opengl = 0;
static int last_func_number = -1;
static size_t(*my_strlen) (const char *) = NULL;

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
        my_strlen = strlen;
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
                argcpy_target_to_host = memcpy_target32_to_host;
            } else {
                wordsize = 64;
                argcpy_target_to_host = memcpy_target64_to_host;
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
                    int mem_state;

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
                    if (args[i]) {
                        mem_state =
                            get_target_mem_state(env, args[i], args_size[i]);
                        if (mem_state == NOT_MAPPED) {
                            fprintf(stderr,
                                    "call %s arg %d pid=%d addr="
                                    TARGET_FMT_lx " size=%d NOT_MAPPED\n",
                                    tab_opengl_calls_name[func_number], i,
                                    pid, args[i], args_size[i]);
                            disconnect_current();
                            return 0;
                        } else if (mem_state == MAPPED_CONTIGUOUS) {
                            saved_out_ptr[i] = 0;
                            args[i] = (arg_t) get_phys_mem_addr(env, args[i]);
                        } else {
                            saved_out_ptr[i] = args[i];
                            args[i] = (arg_t) malloc(args_size[i]);
                        }
                    } else {
                        saved_out_ptr[i] = 0;
                    }
                    break;
                }
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
#ifdef USE_KQEMU
            if (env->kqemu_enabled)
                *(int *) args[1] = 2;
            else
#endif
                *(int *) args[1] = 1;
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
                        if (memcpy_host_to_target(env, saved_out_ptr[i],
                                                (void *) args[i],
                                                args_size[i]) == 0) {
                            fprintf(stderr, "could not copy out parameters "
                                            "back to user space\n");
                            disconnect_current();
                            return 0;
                        }
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
                /* the my_strlen stuff is a hack to workaround a GCC bug if
                 * using directly strlen... */
                if (memcpy_host_to_target(env, target_ret_string, ret_string,
                                        my_strlen(ret_string) + 1) == 0) {
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

#endif
