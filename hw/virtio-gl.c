/*
 *  Virtio GL Device
 *
 *  Copyright (c) 2010 Intel Corporation
 *  Written by:
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

#include "hw.h"
#include "virtio.h"

typedef target_phys_addr_t arg_t;
#include "opengl_process.h"
#include "opengl_exec.h"
#include <sys/time.h>

int decode_call_int(ProcessStruct *p, char *in_args, int args_len, char *r_buffer);

/* Uncomment to enable debugging - WARNING!!! changes ABI! */
//#define DEBUG_GLIO

typedef struct VirtIOGL
{
    VirtIODevice vdev;
    VirtQueue *vq;
} VirtIOGL;

struct d_hdr
{
    int pid;
    int rq_l;
    int rrq_l;
#ifdef DEBUG_GLIO
    int sum;
#endif
};

#define SIZE_OUT_HEADER sizeof(struct d_hdr)

#ifdef DEBUG_GLIO
#define SIZE_IN_HEADER (4*2)
#else
#define SIZE_IN_HEADER 4
#endif

static void virtio_gl_handle(VirtIODevice *vdev, VirtQueue *vq)
{
    VirtQueueElement elem;
    int nkill = 1;

    while(virtqueue_pop(vq, &elem)) {
        struct d_hdr *hdr = (struct d_hdr*)elem.out_sg[0].iov_base;
        ProcessStruct *process;
        int i, remain;
        int ret = 0;

if(!elem.out_num) {
fprintf(stderr, "Bad packet\n");
goto done;
}

process = vmgl_get_process(hdr->pid);

        if(hdr->rq_l) {

            if(hdr->rrq_l) {
                if(process->rq) {  // Usually only the quit packet...
                    free(process->rq);
                    free(process->rrq);
                }
                process->rq  = process->rq_p  = qemu_malloc(hdr->rq_l);
                process->rrq = process->rrq_p = qemu_malloc(hdr->rrq_l);
                process->rq_l  = hdr->rq_l - SIZE_OUT_HEADER;
                process->rrq_l = hdr->rrq_l;
#ifdef DEBUG_GLIO
                process->sum = hdr->sum;
#endif
            }

            i = 0;
            remain = process->rq_l - (process->rq_p - process->rq);
            while(remain && i < elem.out_num){
                char *src = (char *)elem.out_sg[i].iov_base;
                int ilen = elem.out_sg[i].iov_len;
                int len = remain;

                if(i == 0) {
                    src += SIZE_OUT_HEADER;
                    ilen -= SIZE_OUT_HEADER;
                }

                if(len > ilen)
                    len = ilen;

                memcpy(process->rq_p, src, len);
                process->rq_p += len;
                remain -= len;
                i++;
            }

            if(process->rq_p >= process->rq + process->rq_l) {

#ifdef DEBUG_GLIO
            int sum = 0;
            for(i = 0; i < process->rq_l ; i++)
                sum += process->rq[i];
            if(sum != process->sum)
                fprintf(stderr, "Checksum fail\n");
#endif

                *(int*)process->rrq = nkill = decode_call_int(process, 
                                process->rq,      /* command_buffer */
                                process->rq_l,    /* cmd buffer length */
                                process->rrq + SIZE_IN_HEADER);    /* return buffer */

                qemu_free(process->rq);
                process->rq = NULL;

#ifdef DEBUG_GLIO
                sum = 0;
                for(i = SIZE_IN_BUFFER; i < process->rrq_l ; i++)
                    sum += process->rrq[i];
                *(int*)(process->rrq+sizeof(int)) = sum;
#endif

            }
        }

        if(hdr->rrq_l && process->rq == NULL) {

            i = 0;
            remain = process->rrq_l - (process->rrq_p - process->rrq);
            while(remain && i < elem.in_num) {
                char *dst = elem.in_sg[i].iov_base;
                int len = remain;
                int ilen = elem.in_sg[i].iov_len;

                if(len > ilen)
                    len = ilen;

                memcpy(dst, process->rrq_p, len);
                process->rrq_p += len;
                remain -= len;
                ret += len;
                i++;
            }

            if(remain <= 0) {
                qemu_free(process->rrq);
                if(!nkill)
                    disconnect(process);
            }
       }
done:
        virtqueue_push(vq, &elem, ret);

        virtio_notify(vdev, vq);
    }
}

static uint32_t virtio_gl_get_features(VirtIODevice *vdev, uint32_t f)
{
    return 0;
}

static void virtio_gl_save(QEMUFile *f, void *opaque)
{
    VirtIOGL *s = opaque;

    virtio_save(&s->vdev, f);
}

static int virtio_gl_load(QEMUFile *f, void *opaque, int version_id)
{
    VirtIOGL *s = opaque;

    if (version_id != 1)
        return -EINVAL;

    virtio_load(&s->vdev, f);
    return 0;
}

VirtIODevice *virtio_gl_init(DeviceState *dev)
{
    VirtIOGL *s;
    s = (VirtIOGL *)virtio_common_init("virtio-gl", VIRTIO_ID_GL,
                                       0, sizeof(VirtIOGL));

    if (!s)
        return NULL;

    s->vdev.get_features = virtio_gl_get_features;

    s->vq = virtio_add_queue(&s->vdev, 128, virtio_gl_handle);

    register_savevm(dev, "virtio-gl", -1, 1, virtio_gl_save, virtio_gl_load, s);

    return &s->vdev;
}

