#ifndef MIPMAPS_H
#define MIPMAPS_H

#include <webgpu.h>

int mipmaps_get_count(const int texture_width, const int texture_height);

void mipmaps_generate(WGPUDevice device, WGPUShaderModule module, WGPUSampler sampler, WGPUTexture texture, int mip_level_count);

#endif // MIPMAPS_H
