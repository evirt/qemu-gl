/*
 *  Copyright (c) 2010 Intel
 *  Written by:
 *    Ian Molton <ian.molton@collabora.co.uk>
 *
 */

typedef struct {
    int process_id;
    int wordsize;
    int rq_l, rrq_l;
    int sum; // Debugging only
    char *rq, *rq_p;
    char *rrq, *rrq_p;
} ProcessStruct;

