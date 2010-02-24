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
//#include "qemu-char.h"
#include "virtio.h"
//#include "virtio-rng.h"
//#include "rng.h"
#include <sys/time.h>

typedef struct VirtIOGL
{
    VirtIODevice vdev;
    VirtQueue *vq;
//    CharDriverState *chr;
//    struct timeval last;
//    int rate;
//    int egd;
//    int entropy_remaining;
//    int pool;
} VirtIOGL;

extern int virtio_opengl_link(char *buffer);

static void virtio_gl_handle(VirtIODevice *vdev, VirtQueue *vq)
{
	VirtQueueElement elem;

//	fprintf(stderr, "handle_gl\n");
    while (virtqueue_pop(vq, &elem)) {
        size_t ret;

        /* The guest always sends only one sg */
//        fprintf(stderr, " %08x %d\n", (unsigned int)(long)elem.out_sg[0].iov_base,
//                                    (int)elem.out_sg[0].iov_len);

//	fprintf(stderr, "%s\n", (char *)elem.out_sg[0].iov_base);
	virtio_opengl_link((char *)elem.out_sg[0].iov_base);
	
	ret = elem.out_sg[0].iov_len;

        virtqueue_push(vq, &elem, ret);
    }
    virtio_notify(vdev, vq);

    // Write the result back...



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

	fprintf(stderr, "Init GL dev\n");
    if (!s)
        return NULL;

    s->vdev.get_features = virtio_gl_get_features;

    s->vq = virtio_add_queue(&s->vdev, 128, virtio_gl_handle);
//    s->chr = rngdev->chrdev;
 //   s->rate = rngdev->rate;
//    gettimeofday(&s->last, NULL);

//    if(rngdev->proto && !strncmp(rngdev->proto, "egd", 3))
//        s->egd = 1;

#ifdef DEBUG
//    printf("entropy being read from %s", rngdev->chrdev->label);
//    if(s->rate)
//        printf(" at %d bytes/sec max.", s->rate);
//    printf(" protocol: %s\n", s->egd?"egd":"raw");
#endif

//    qemu_chr_add_handlers(s->chr, vgl_can_read, vgl_read, vgl_event, s);

    register_savevm("virtio-gl", -1, 1, virtio_gl_save, virtio_gl_load, s);

    return &s->vdev;
}

