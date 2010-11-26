
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
\n                                                                            \
\ninline void gl_passingthrough_call (int process_id, char *in_arg,           \
\n                                    int in_arg_length)                      \
\n{                                                                           \
\n    ProcessStruct *process = vmgl_get_process (process_id);                 \
\n    decode_call_int (process, in_arg, in_arg_length, r_buffer);             \
\n}                                                                           \
\n                                                                            \
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

static inline int is_string_arguments (int func_number)
{
    return ((func_number == glShaderSource_fake_func));
}

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

static void print_askii_char (FILE *fp, unsigned char chr)
{
    if ((chr >= 0x20) && (chr <= 0x7e)) {
        fprintf (fp, "%c", chr);
    } else if (chr == 0) {
        fprintf (fp, "\'\\0\'");
    } else if (chr == 0x0a) {
        fprintf (fp, "\'\\n\'");
    } else {
        fprintf (fp, "0x%x%x ", chr >> 4, chr & 0xf);
    }
}

static void dump_data (FILE *fp, unsigned char *buf, int size, int dump_as_askii)
{
    unsigned char *ptr = buf;
    int i;

    if (size <= 0) {
        return;
    }

    // begining of a line
    if (!dump_as_askii) {
        fprintf (fp, "    ");
    }

    for (i = 0; i < size - 1; ++ i) {
        if (dump_as_askii) {
            print_askii_char (fp, *ptr);
        } else {
            fprintf (fp, "0x%x%x, ", *ptr >> 4, *ptr & 0xf);
        }
        ptr ++;
        if (!dump_as_askii && (((i+1) & 0xf) == 0)) {
            fprintf (fp, "\n    ");
        }
    }

    if (dump_as_askii) {
        print_askii_char (fp, *ptr);
    } else {
        fprintf (fp, "0x%x%x", *ptr >> 4, *ptr &0xf);
    }
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
    fprintf (dec_input_f, "    gl_passingthrough_call (%d, in_args_%d, %d);\t\t//%s\n", process->process_id, decoding_count, args_len, tab_opengl_calls_name[func_number]);
    fprintf (dec_input_data_f, "//arguments for function %d\n", decoding_count);

    // dump in arguments as askii code and for reference.
    if (is_string_arguments (func_number)) {
        fprintf (dec_input_data_f, "#if 0\n");
        fprintf (dec_input_data_f, "char in_args_%d[%d]={\n", decoding_count, args_len);
        dump_data (dec_input_data_f, (unsigned char *)in_args, args_len, 1);
        fprintf (dec_input_data_f, "};\n");
        fprintf (dec_input_data_f, "#endif\n");
    }
    // in_args
    fprintf (dec_input_data_f, "char in_args_%d[%d]={\n", decoding_count, args_len);
    dump_data (dec_input_data_f, (unsigned char *)in_args, args_len, 0);
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
