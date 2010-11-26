
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include "osdep.h"
#include "opengl_func.h"
#include "opengl_process.h"
#include "opengl_exec.h"

static int log_gl_passing_through = 0;
static int decoding_count = -1;
static FILE *dec_input_f = NULL;
static FILE *dec_input_data_f = NULL;
//static FILE *dec_output_f = NULL;

const char *input_arg_header = "                                              \
\n/* This file is generated automatically                                     \
\n * to reproduce problem caused by gl call sequence                          \
\n * forwarded from qemu client OS.                                           \
\n * Locations pointed by data pointers from gl function calls are stored     \
\n * here.                                                                    \
\n * Data are resumed to be *little* endian.                                  \
\n *                                                                          \
\n */                                                                         \
\n";

const char *input_arg_tail = "                                                \
";

const char *input_log_header = "                                              \
\n/* This file is generated automatically                                     \
\n * to reproduce problem caused by gl call sequence                          \
\n * forwarded from qemu client OS                                            \
\n */                                                                         \
\n#include \"opengl_process.h\"                                               \
\n#include \"decoding_input_arguments.h\"                                     \
\n// temporary buffer to hold the return value. Need to make sure it will not \
\n// overflow.                                                                \
\nextern int show_offscr_window;                                              \
\nstatic char r_buffer[4096*4096];                                            \
\nint main ()                                                                 \
\n{                                                                           \
\n    ProcessStruct *process;                                                 \
\n    show_offscr_window = 1;                                                 \
\n    disable_decoding_log ();                                                \
\n    // start the opengl call                                                \
\n";

const char *input_log_tail = "                                                \
\n}                                                                           \
\n";

void log_initialize (void)
{
    dec_input_f = fopen("/tmp/gloffscreen_test.c", "w");
    dec_input_data_f = fopen("/tmp/decoding_input_arguments.h", "w");

    fprintf (dec_input_f, input_log_header);
    fprintf (dec_input_data_f, input_arg_header);
    decoding_count = 0;
}

void log_finalize (void)
{
    if (dec_input_f) {
        fprintf (dec_input_f, input_log_tail);
        fclose (dec_input_f);
        dec_input_f = NULL;
    }
    if (dec_input_data_f) {
        fprintf (dec_input_data_f, input_arg_tail);
        fclose (dec_input_data_f);
        dec_input_data_f = NULL;
    }
}

static void dump_data (FILE *fp, unsigned char *buf, int size)
{
    unsigned char *ptr = buf;
    int i;

    if (size <= 0) {
        return;
    }

    fprintf (fp, "    ");
    for (i = 0; i < size - 1; ++ i) {
        fprintf (fp, "0x%x%x, ", *ptr >> 4, *ptr & 0xf);
        ptr ++;
        if (((i+1) & 0xf) == 0) {
            fprintf (fp, "\n    ");
        }
    }

    fprintf (fp, "0x%x%x", *ptr >> 4, *ptr &0xf);
}

void log_decoding_input (ProcessStruct *process, char *in_args, int args_len)
{
    int func_number = *(short*)in_args;
    if (!decoding_log_enabled ()) {
        return 0;
    }
    if (decoding_count == -1) {
        log_initialize ();
    }
    ++ decoding_count;
    // start to record inputs.
    fprintf (dec_input_f, "    process = vmgl_get_process(%d);\n", process->process_id);
    fprintf (dec_input_f, "    decode_call_int ((ProcessStruct *)process, in_args_%d, %d, r_buffer);\n", decoding_count, args_len);
    fprintf (dec_input_data_f, "//arguments for function %d\n", decoding_count);

    // in_args
    fprintf (dec_input_data_f, "char in_args_%d[%d]={\n", decoding_count, args_len);
    dump_data (dec_input_data_f, (unsigned char *)in_args, args_len);
    fprintf (dec_input_data_f, "};\n");
}

void log_decoding_output (char *r_buffer)
{
}

void enable_decoding_log (void)
{
    log_gl_passing_through = 1;
}

void disable_decoding_log (void)
{
    log_gl_passing_through = 0;
}

int decoding_log_enabled (void)
{
    return (log_gl_passing_through == 1);
}
