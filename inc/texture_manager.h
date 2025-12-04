#ifndef TEXTURE_MANAGER_H
#define TEXTURE_MANAGER_H

#include <stdlib.h>
#include <webgpu.h>
#include <unordered_map>

struct _State;
typedef _State State;

struct CachedTexture {
    WGPUTexture texture;
    WGPUTextureView view;
};

struct TextureManager {
    WGPUDevice device;
    WGPUQueue queue;
    WGPUSampler sampler;
    std::unordered_map<const char*,CachedTexture> texture_cache;
};

void texture_manager_init(const State *s, TextureManager *tm);

WGPUTextureView texture_manager_get_view(const TextureManager *tm, const char *path);

// returns a white square
WGPUTextureView texture_manager_get_view_identity(const TextureManager *tm);

void texture_manager_terminate(TextureManager *tm);

#endif
