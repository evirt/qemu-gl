extern void init_process_tab(void);
extern void disconnect(ProcessStruct *process);
extern ProcessStruct *vmgl_context_switch(pid_t pid, int switch_gl_context);
extern int do_function_call(ProcessStruct *process, int func_number,
                            arg_t *args, char *ret_string);

