//#include "../cpu-defs.h"

typedef struct {
    int process_id;
    int (*argcpy_target_to_host) (CPUState *, void *, target_ulong, int);
} ProcessStruct;
