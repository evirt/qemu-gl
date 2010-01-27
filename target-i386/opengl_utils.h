/*
 *  Functions used by host & client sides
 *
 *  Copyright (c) 2007 Even Rouault
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


#ifndef _OPENGL_UTILS
#define _OPENGL_UTILS

typedef struct {
    unsigned int *values;
    int nbValues;
} RangeAllocator;

/*
static void print_range(RangeAllocator* range)
{
  int i;
  printf("%s", "table : ");
  for(i=0;i<range->nbValues;i++)
  {
    printf("%d ", range->values[i]);
  }
  printf("\n");
}
*/

static inline void alloc_value(RangeAllocator *range, unsigned int value)
{
    if (value == 0)
        return;
    if (range->nbValues >= 1) {
        int lower = 0;
        int upper = range->nbValues - 1;

        while (1) {
            int mid = (lower + upper) / 2;

            if (range->values[mid] > value)
                upper = mid;
            else if (range->values[mid] < value)
                lower = mid;
            else
                break;
            if (upper - lower <= 1) {
                if (value < range->values[lower]) {
                    range->values =
                        realloc(range->values,
                                (range->nbValues + 1) * sizeof(int));
                    memmove(&range->values[lower + 1], &range->values[lower],
                            (range->nbValues - lower) * sizeof(int));
                    range->values[lower] = value;
                    range->nbValues++;
                } else if (value == range->values[lower]) {
                } else if (value < range->values[upper]) {
                    range->values =
                        realloc(range->values,
                                (range->nbValues + 1) * sizeof(int));
                    memmove(&range->values[upper + 1], &range->values[upper],
                            (range->nbValues - upper) * sizeof(int));
                    range->values[upper] = value;
                    range->nbValues++;
                } else if (value == range->values[upper]) {
                } else {
                    upper++;

                    range->values =
                        realloc(range->values,
                                (range->nbValues + 1) * sizeof(int));
                    memmove(&range->values[upper + 1], &range->values[upper],
                            (range->nbValues - upper) * sizeof(int));
                    range->values[upper] = value;
                    range->nbValues++;
                }
                break;
            }
        }
    } else {
        range->values = malloc(sizeof(int));
        range->values[0] = value;
        range->nbValues = 1;
    }
}

/* return first value */
static inline unsigned int alloc_range(RangeAllocator *range, int n,
                unsigned int *values)
{
    int i, j;

    if (range->nbValues == 0) {
        range->nbValues = n;
        range->values = malloc(n * sizeof(int));
        for (i = 0; i < n; i++) {
            range->values[i] = i + 1;
            if (values)
                values[i] = range->values[i];
        }
        return 1;
    } else {
        int lastValue = 1;

        for (i = 0; i < range->nbValues; i++) {
            if ((int) range->values[i] - (int) lastValue - 1 >= n) {
                range->values =
                    realloc(range->values,
                            (range->nbValues + n) * sizeof(int));
                memmove(&range->values[i + n], &range->values[i],
                        (range->nbValues - i) * sizeof(int));
                for (j = 0; j < n; j++) {
                    range->values[i + j] = lastValue + 1 + j;
                    if (values)
                        values[j] = range->values[i + j];
                }
                range->nbValues += n;
                break;
            } else
                lastValue = range->values[i];
        }
        if (i == range->nbValues) {
            range->values =
                realloc(range->values, (range->nbValues + n) * sizeof(int));
            for (j = 0; j < n; j++) {
                range->values[i + j] = lastValue + 1 + j;
                if (values)
                    values[j] = range->values[i + j];
            }
            range->nbValues += n;
        }
        return lastValue + 1;
    }
}

static inline void delete_value(RangeAllocator *range, unsigned int value)
{
    if (value == 0)
        return;
    if (range->nbValues >= 1) {
        int lower = 0;
        int upper = range->nbValues - 1;

        while (1) {
            int mid = (lower + upper) / 2;

            if (range->values[mid] > value)
                upper = mid;
            else if (range->values[mid] < value)
                lower = mid;
            else {
                lower = upper = mid;
            }
            if (upper - lower <= 1) {
                if (value == range->values[lower]) {
                    memmove(&range->values[lower], &range->values[lower + 1],
                            (range->nbValues - lower - 1) * sizeof(int));
                    range->nbValues--;
                } else if (value == range->values[upper]) {
                    memmove(&range->values[upper], &range->values[upper + 1],
                            (range->nbValues - upper - 1) * sizeof(int));
                    range->nbValues--;
                }
                break;
            }
        }
    }
}

static inline void delete_range(RangeAllocator *range, int n,
                const unsigned int *values)
{
    int i;

    for (i = 0; i < n; i++) {
        delete_value(range, values[i]);
    }
}

static inline void delete_consecutive_values(RangeAllocator *range,
                unsigned int first, int n)
{
    int i;

    for (i = 0; i < n; i++) {
        delete_value(range, first + i);
    }
}


/*****************************************************************/
/*                                                               */
/* CRC LOOKUP TABLE                                              */
/* ================                                              */
/* The following CRC lookup table was generated automagically    */
/* by the Rocksoft^tm Model CRC Algorithm Table Generation       */
/* Program V1.0 using the following model parameters:            */
/*                                                               */
/*    Width   : 4 bytes.                                         */
/*    Poly    : 0x04C11DB7L                                      */
/*    Reverse : TRUE.                                            */
/*                                                               */
/* For more information on the Rocksoft^tm Model CRC Algorithm,  */
/* see the document titled "A Painless Guide to CRC Error        */
/* Detection Algorithms" by Ross Williams                        */
/* (ross@guest.adelaide.edu.au.). This document is likely to be  */
/* in the FTP archive "ftp.adelaide.edu.au/pub/rocksoft".        */
/*                                                               */
/*****************************************************************/

static const unsigned int crctable[256] = {
    0x00000000, 0x77073096, 0xEE0E612C, 0x990951BA,
    0x076DC419, 0x706AF48F, 0xE963A535, 0x9E6495A3,
    0x0EDB8832, 0x79DCB8A4, 0xE0D5E91E, 0x97D2D988,
    0x09B64C2B, 0x7EB17CBD, 0xE7B82D07, 0x90BF1D91,
    0x1DB71064, 0x6AB020F2, 0xF3B97148, 0x84BE41DE,
    0x1ADAD47D, 0x6DDDE4EB, 0xF4D4B551, 0x83D385C7,
    0x136C9856, 0x646BA8C0, 0xFD62F97A, 0x8A65C9EC,
    0x14015C4F, 0x63066CD9, 0xFA0F3D63, 0x8D080DF5,
    0x3B6E20C8, 0x4C69105E, 0xD56041E4, 0xA2677172,
    0x3C03E4D1, 0x4B04D447, 0xD20D85FD, 0xA50AB56B,
    0x35B5A8FA, 0x42B2986C, 0xDBBBC9D6, 0xACBCF940,
    0x32D86CE3, 0x45DF5C75, 0xDCD60DCF, 0xABD13D59,
    0x26D930AC, 0x51DE003A, 0xC8D75180, 0xBFD06116,
    0x21B4F4B5, 0x56B3C423, 0xCFBA9599, 0xB8BDA50F,
    0x2802B89E, 0x5F058808, 0xC60CD9B2, 0xB10BE924,
    0x2F6F7C87, 0x58684C11, 0xC1611DAB, 0xB6662D3D,
    0x76DC4190, 0x01DB7106, 0x98D220BC, 0xEFD5102A,
    0x71B18589, 0x06B6B51F, 0x9FBFE4A5, 0xE8B8D433,
    0x7807C9A2, 0x0F00F934, 0x9609A88E, 0xE10E9818,
    0x7F6A0DBB, 0x086D3D2D, 0x91646C97, 0xE6635C01,
    0x6B6B51F4, 0x1C6C6162, 0x856530D8, 0xF262004E,
    0x6C0695ED, 0x1B01A57B, 0x8208F4C1, 0xF50FC457,
    0x65B0D9C6, 0x12B7E950, 0x8BBEB8EA, 0xFCB9887C,
    0x62DD1DDF, 0x15DA2D49, 0x8CD37CF3, 0xFBD44C65,
    0x4DB26158, 0x3AB551CE, 0xA3BC0074, 0xD4BB30E2,
    0x4ADFA541, 0x3DD895D7, 0xA4D1C46D, 0xD3D6F4FB,
    0x4369E96A, 0x346ED9FC, 0xAD678846, 0xDA60B8D0,
    0x44042D73, 0x33031DE5, 0xAA0A4C5F, 0xDD0D7CC9,
    0x5005713C, 0x270241AA, 0xBE0B1010, 0xC90C2086,
    0x5768B525, 0x206F85B3, 0xB966D409, 0xCE61E49F,
    0x5EDEF90E, 0x29D9C998, 0xB0D09822, 0xC7D7A8B4,
    0x59B33D17, 0x2EB40D81, 0xB7BD5C3B, 0xC0BA6CAD,
    0xEDB88320, 0x9ABFB3B6, 0x03B6E20C, 0x74B1D29A,
    0xEAD54739, 0x9DD277AF, 0x04DB2615, 0x73DC1683,
    0xE3630B12, 0x94643B84, 0x0D6D6A3E, 0x7A6A5AA8,
    0xE40ECF0B, 0x9309FF9D, 0x0A00AE27, 0x7D079EB1,
    0xF00F9344, 0x8708A3D2, 0x1E01F268, 0x6906C2FE,
    0xF762575D, 0x806567CB, 0x196C3671, 0x6E6B06E7,
    0xFED41B76, 0x89D32BE0, 0x10DA7A5A, 0x67DD4ACC,
    0xF9B9DF6F, 0x8EBEEFF9, 0x17B7BE43, 0x60B08ED5,
    0xD6D6A3E8, 0xA1D1937E, 0x38D8C2C4, 0x4FDFF252,
    0xD1BB67F1, 0xA6BC5767, 0x3FB506DD, 0x48B2364B,
    0xD80D2BDA, 0xAF0A1B4C, 0x36034AF6, 0x41047A60,
    0xDF60EFC3, 0xA867DF55, 0x316E8EEF, 0x4669BE79,
    0xCB61B38C, 0xBC66831A, 0x256FD2A0, 0x5268E236,
    0xCC0C7795, 0xBB0B4703, 0x220216B9, 0x5505262F,
    0xC5BA3BBE, 0xB2BD0B28, 0x2BB45A92, 0x5CB36A04,
    0xC2D7FFA7, 0xB5D0CF31, 0x2CD99E8B, 0x5BDEAE1D,
    0x9B64C2B0, 0xEC63F226, 0x756AA39C, 0x026D930A,
    0x9C0906A9, 0xEB0E363F, 0x72076785, 0x05005713,
    0x95BF4A82, 0xE2B87A14, 0x7BB12BAE, 0x0CB61B38,
    0x92D28E9B, 0xE5D5BE0D, 0x7CDCEFB7, 0x0BDBDF21,
    0x86D3D2D4, 0xF1D4E242, 0x68DDB3F8, 0x1FDA836E,
    0x81BE16CD, 0xF6B9265B, 0x6FB077E1, 0x18B74777,
    0x88085AE6, 0xFF0F6A70, 0x66063BCA, 0x11010B5C,
    0x8F659EFF, 0xF862AE69, 0x616BFFD3, 0x166CCF45,
    0xA00AE278, 0xD70DD2EE, 0x4E048354, 0x3903B3C2,
    0xA7672661, 0xD06016F7, 0x4969474D, 0x3E6E77DB,
    0xAED16A4A, 0xD9D65ADC, 0x40DF0B66, 0x37D83BF0,
    0xA9BCAE53, 0xDEBB9EC5, 0x47B2CF7F, 0x30B5FFE9,
    0xBDBDF21C, 0xCABAC28A, 0x53B39330, 0x24B4A3A6,
    0xBAD03605, 0xCDD70693, 0x54DE5729, 0x23D967BF,
    0xB3667A2E, 0xC4614AB8, 0x5D681B02, 0x2A6F2B94,
    0xB40BBE37, 0xC30C8EA1, 0x5A05DF1B, 0x2D02EF8D
};

static unsigned int calc_checksum(const void *_ptr, int length,
                                  unsigned int seed)
{
    int i;
    unsigned int crc = seed;
    unsigned char *ptr = (unsigned char *) _ptr;

    if (ptr == NULL)
        return -1;
    for (i = 0; i < length; i++) {
        crc = crctable[(crc ^ *ptr++) & 0xFF] ^ (crc >> 8);
    }
    return crc;
}


static inline void display_gl_call(FILE *f,
                int func_number, arg_t *args, int *args_size)
{
    int i;

    if (func_number < 0) {
        fprintf(f, "unknown call : %d\n", func_number);
        return;
    }
    Signature *signature = (Signature *) tab_opengl_calls[func_number];
    int nb_args = signature->nb_args;
    int *args_type = signature->args_type;

    fprintf(f, "%s(", tab_opengl_calls_name[func_number]);

    for (i = 0; i < nb_args; i++) {
        switch (args_type[i]) {
        case TYPE_UNSIGNED_CHAR:
        case TYPE_CHAR:
            {
                fprintf(f, "%d", (char) args[i]);
                break;
            }

        case TYPE_UNSIGNED_SHORT:
        case TYPE_SHORT:
            {
                fprintf(f, "%d", (short) args[i]);
                break;
            }

        case TYPE_UNSIGNED_INT:
        case TYPE_INT:
            {
                fprintf(f, "%d", (int) args[i]);
                break;
            }

        case TYPE_FLOAT:
            fprintf(f, "%f", *(float *) &args[i]);
            break;

        case TYPE_DOUBLE:
          CASE_IN_KNOWN_SIZE_POINTERS:
        case TYPE_NULL_TERMINATED_STRING:
          CASE_IN_UNKNOWN_SIZE_POINTERS:
          CASE_IN_LENGTH_DEPENDING_ON_PREVIOUS_ARGS:
            if (args_type[i] == TYPE_NULL_TERMINATED_STRING) {
                fprintf(f, "\"%s\"", (char *) args[i]);
            } else if (args_type[i] == TYPE_DOUBLE)
                fprintf(f, "%f", *(double *) args[i]);
            else if (IS_ARRAY_CHAR(args_type[i]) &&
                     args_size[i] <= 4 * sizeof(char)) {
                int j;
                int n = args_size[i] / sizeof(char);

                fprintf(f, "(");
                for (j = 0; j < n; j++) {
                    fprintf(f, "%d", ((unsigned char *) args[i])[j]);
                    if (j != n - 1)
                        fprintf(f, ", ");
                }
                fprintf(f, ")");
            } else if (IS_ARRAY_SHORT(args_type[i]) &&
                       args_size[i] <= 4 * sizeof(short)) {
                int j;
                int n = args_size[i] / sizeof(short);

                fprintf(f, "(");
                for (j = 0; j < n; j++) {
                    fprintf(f, "%d", ((short *) args[i])[j]);
                    if (j != n - 1)
                        fprintf(f, ", ");
                }
                fprintf(f, ")");
            } else if (IS_ARRAY_INT(args_type[i]) &&
                       args_size[i] <= 4 * sizeof(int)) {
                int j;
                int n = args_size[i] / sizeof(int);

                fprintf(f, "(");
                for (j = 0; j < n; j++) {
                    fprintf(f, "%d", ((int *) args[i])[j]);
                    if (j != n - 1)
                        fprintf(f, ", ");
                }
                fprintf(f, ")");
            } else if (IS_ARRAY_FLOAT(args_type[i]) &&
                       args_size[i] <= 4 * sizeof(float)) {
                int j;
                int n = args_size[i] / sizeof(float);

                fprintf(f, "(");
                for (j = 0; j < n; j++) {
                    fprintf(f, "%f", ((float *) args[i])[j]);
                    if (j != n - 1)
                        fprintf(f, ", ");
                }
                fprintf(f, ")");
            } else if (IS_ARRAY_DOUBLE(args_type[i]) &&
                       args_size[i] <= 4 * sizeof(double)) {
                int j;
                int n = args_size[i] / sizeof(double);

                fprintf(f, "(");
                for (j = 0; j < n; j++) {
                    fprintf(f, "%f", ((double *) args[i])[j]);
                    if (j != n - 1)
                        fprintf(f, ", ");
                }
                fprintf(f, ")");
            } else {
                fprintf(f, "%d bytes", args_size[i]);
                fprintf(f, "(crc = 0x%x)",
                        calc_checksum((void *) args[i], args_size[i],
                                      0xFFFFFFFF));
            }
            break;

          CASE_OUT_LENGTH_DEPENDING_ON_PREVIOUS_ARGS:
          CASE_OUT_UNKNOWN_SIZE_POINTERS:
          CASE_OUT_KNOWN_SIZE_POINTERS:
            {
                fprintf(f, "%d bytes (OUT)", args_size[i]);
                break;
            }

        case TYPE_IN_IGNORED_POINTER:
            break;

        default:
            fprintf(f, "shouldn't happen : call %s arg %d\n",
                    tab_opengl_calls_name[func_number], i);
            return;
            break;
        }
        if (i < nb_args - 1)
            fprintf(f, ", ");
    }
    fprintf(f, ")\n");
}

#endif
