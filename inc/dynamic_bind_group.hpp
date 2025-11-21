#ifndef TEMP
#define TEMP

#include <webgpu.h>
#include "constants.h"

typedef struct DynamicBindGroup {
    WGPUBindGroupLayout layout;
    WGPUBindGroupEntry entries[BG_COMP_ENTRY_COUNT];
} DynamicBindGroup;

#endif
