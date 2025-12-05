#ifndef TEXTURE_MANAGER_H
#define TEXTURE_MANAGER_H

#include <stdlib.h>
#include <webgpu.h>
#include <unordered_map>
#include <string>

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
    std::unordered_map<std::string, CachedTexture> texture_cache;
};

void texture_manager_init(const State *s, TextureManager *tm);

WGPUTextureView texture_manager_get_view(TextureManager *tm, std::string path);

// returns a white square
WGPUTextureView texture_manager_get_view_identity(TextureManager *tm);

void texture_manager_terminate(TextureManager *tm);

#endif
