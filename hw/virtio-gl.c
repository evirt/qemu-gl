/*
 * Virtio GL Device
 *
 * Copyright Collabora 2009
 *
 * Authors:
 *  Ian Molton <ian.molton@collabora.co.uk>
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 *
 */

#include "hw.h"
#include "virtio.h"
#include <sys/time.h>

int decode_call_int(int pid, char *in_args, int args_len, char *r_buffer);


typedef struct VirtIOGL
{
    VirtIODevice vdev;
    VirtQueue *vq;
} VirtIOGL;

#define SIZE_OUT_HEADER (4*3)
#define SIZE_IN_HEADER 4

static void virtio_gl_handle(VirtIODevice *vdev, VirtQueue *vq)
{
    VirtQueueElement elem;

    while(virtqueue_pop(vq, &elem)) {
        int i;
        int length = ((int*)elem.out_sg[0].iov_base)[1];
        int r_length = ((int*)elem.out_sg[0].iov_base)[2];
	int ret = 0;
        char *buffer, *ptr;
        char *r_buffer = NULL;
	int *i_buffer;

	if(length < SIZE_OUT_HEADER || r_length < SIZE_IN_HEADER)
		goto done;

	buffer = qemu_malloc(length);
        r_buffer = qemu_malloc(r_length);
	ptr = buffer;
        i_buffer = (int*)buffer;

        i = 0;
        while(length){
            int next = length;
            if(next > elem.out_sg[i].iov_len)
                next = elem.out_sg[i].iov_len;
            memcpy(ptr, (char *)elem.out_sg[i].iov_base, next);
            ptr += next;
            ret += next;
            i++;
            length -= next;
        }

        *(int*)r_buffer = decode_call_int(i_buffer[0], /* pid */
                        buffer + SIZE_OUT_HEADER, /* command_buffer */
                        i_buffer[1] - SIZE_OUT_HEADER, /* cmd buffer length */
                        r_buffer + SIZE_IN_HEADER);    /* return buffer */

        qemu_free(buffer);

	if(r_length)
		ret = r_length;
        i = 0;
        ptr = r_buffer;
        while(r_length) {
            int next = r_length;
            if(next > elem.in_sg[i].iov_len)
                next = elem.in_sg[i].iov_len;
            memcpy(elem.in_sg[i].iov_base, ptr, next);
            ptr += next;
            r_length -= next;
            i++;
        }

        qemu_free(r_buffer);
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
    s = (VirtIOGL *)virtio_common_init("virtio-gl",
                                            VIRTIO_ID_GL,
                                            0, sizeof(VirtIOGL));

    if (!s)
        return NULL;

    s->vdev.get_features = virtio_gl_get_features;

    s->vq = virtio_add_queue(&s->vdev, 128, virtio_gl_handle);

    register_savevm(dev, "virtio-gl", -1, 1, virtio_gl_save, virtio_gl_load, s);

    return &s->vdev;
}

