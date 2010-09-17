/*
 *  Copyright (c) 2006,2007 Even Rouault
 *  Copyright (c) 2010 Intel
 *  Written by:
 *    Gordon Williams <gordon.williams@collabora.co.uk>
 *    Ian Molton <ian.molton@collabora.co.uk>
 *
 */


#include <sys/types.h>

extern void disconnect(ProcessStruct *process);
extern ProcessStruct *vmgl_get_process(pid_t pid);
extern void vmgl_context_switch(ProcessStruct *process, int switch_gl_context);
extern int do_function_call(ProcessStruct *process, int func_number,
                            arg_t *args, char *ret_string);

