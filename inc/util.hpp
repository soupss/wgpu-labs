#ifndef UTIL_H
#define UTIL_H

#include <webgpu.h>
#include <stdio.h>
#include <stdlib.h>

void load_spirv(const char* path, uint32_t** out_data, int* out_word_count);
void print_adapter_info(WGPUAdapter adapter);
void print_device_info(WGPUDevice device);

#endif
