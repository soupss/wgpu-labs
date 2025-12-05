#include "texture_manager.h"
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#include "state.h"

//TODO: mipmaps

void texture_manager_init(const State *s, TextureManager *tm) {
    tm->device = s->device;
    tm->queue = s->queue;

    new (&tm->texture_cache) std::unordered_map<std::string, CachedTexture>();

    WGPUSamplerDescriptor sampler_desc = {
        .addressModeU = WGPUAddressMode_Repeat,
        .addressModeV = WGPUAddressMode_Repeat,
        .addressModeW = WGPUAddressMode_Repeat,
        .magFilter = WGPUFilterMode_Linear,
        .minFilter = WGPUFilterMode_Linear,
        .mipmapFilter = WGPUMipmapFilterMode_Linear,
        .lodMinClamp = 0.0f,
        .lodMaxClamp = 1000.0f,
        .compare = WGPUCompareFunction_Undefined,
        .maxAnisotropy = 16
    };
    tm->sampler = wgpuDeviceCreateSampler(s->device, &sampler_desc);
}

WGPUTextureView texture_manager_get_view(TextureManager *tm, std::string path) {
    CachedTexture t = {0};

    auto search = tm->texture_cache.find(path);
    if (search != tm->texture_cache.end()) { // texture cached
        t = search->second;
        printf("used cached view %s\n", path.c_str());
    }
    else {
        // load pixels
        int w,h;
        const int channels = 4;
        stbi_set_flip_vertically_on_load(true);
        unsigned char *pixels = stbi_load(path.c_str(), &w, &h, NULL, channels);
        if (!pixels) {
            fprintf(stderr, "Failed to load image %s: %s!\n", path.c_str(), stbi_failure_reason());
            return NULL;
        }

        // create texture
        WGPUExtent3D size = {(uint32_t)w, (uint32_t)h, 1};
        WGPUTextureDescriptor tex_desc = {
            .label = {path.c_str(), WGPU_STRLEN},
            .size = size,
            .sampleCount = 1,
            .mipLevelCount = 1,
            .format = WGPUTextureFormat_RGBA8Unorm,
            .usage = WGPUTextureUsage_TextureBinding | WGPUTextureUsage_CopyDst | WGPUTextureUsage_StorageBinding,
        };

        t.texture = wgpuDeviceCreateTexture(tm->device, &tex_desc);

        // write pixels to texture
        WGPUTexelCopyTextureInfo dest = {
            .texture = t.texture,
            .mipLevel = 0,
            .origin = {0, 0, 0},
            .aspect = WGPUTextureAspect_All
        };
        WGPUTexelCopyBufferLayout data_layout = {
            .offset = 0,
            .bytesPerRow = (uint32_t)(w * channels * sizeof(unsigned char)),
            .rowsPerImage = (uint32_t)h
        };

        wgpuQueueWriteTexture(tm->queue, &dest, pixels, w * h * channels * sizeof(unsigned char), &data_layout, &size);
        stbi_image_free(pixels);

        // create view
        WGPUTextureViewDescriptor view_desc = {
            .baseMipLevel = 0,
            .mipLevelCount = 1,
            .baseArrayLayer = 0,
            .arrayLayerCount = 1,
            .format = WGPUTextureFormat_RGBA8Unorm,
            .dimension = WGPUTextureViewDimension_2D,
            .aspect = WGPUTextureAspect_All,
        };
        t.view = wgpuTextureCreateView(t.texture, &view_desc);

        printf("caching %s\n", path.c_str());
        tm->texture_cache.insert({path, t});
    }
    return t.view;
}

// returns a white square view
WGPUTextureView texture_manager_get_view_identity(TextureManager *tm) {
    CachedTexture t = {0};

    auto search = tm->texture_cache.find("identity");
    if (search != tm->texture_cache.end()) { // texture cached
        t = search->second;
        printf("used cached identity view\n");
    }
    else {
        // create texture
        WGPUExtent3D size = {1, 1, 1};
        WGPUTextureDescriptor tex_desc = {
            .label = {"Texture map identity", WGPU_STRLEN},
            .size = size,
            .sampleCount = 1,
            .mipLevelCount = 1,
            .dimension = WGPUTextureDimension_2D,
            .format = WGPUTextureFormat_RGBA8Unorm,
            .usage = WGPUTextureUsage_TextureBinding | WGPUTextureUsage_CopyDst,
        };

        t.texture = wgpuDeviceCreateTexture(tm->device, &tex_desc);

        // write pixel to texture
        WGPUTexelCopyTextureInfo dest = {
            .texture = t.texture,
            .mipLevel = 0,
            .origin = {0, 0, 0},
            .aspect = WGPUTextureAspect_All
        };
        WGPUTexelCopyBufferLayout data_layout = {
            .offset = 0,
            .bytesPerRow = 4 * sizeof(char),
            .rowsPerImage = 1
        };

        unsigned char pixel[4] = {255, 255, 255, 255};
        wgpuQueueWriteTexture(tm->queue, &dest, pixel, 4*sizeof(char), &data_layout, &size);

        // create view
        WGPUTextureViewDescriptor view_desc = {
            .baseMipLevel = 0,
            .mipLevelCount = 1,
            .baseArrayLayer = 0,
            .arrayLayerCount = 1,
            .format = WGPUTextureFormat_RGBA8Unorm,
            .dimension = WGPUTextureViewDimension_2D,
            .aspect = WGPUTextureAspect_All,
        };
        t.view = wgpuTextureCreateView(t.texture, &view_desc);
        // add to cache
        printf("caching identity\n");
        std::string key = "identity";
        tm->texture_cache.insert({key, t});
    }
    return t.view;
}

void texture_manager_terminate(TextureManager *tm) {
    wgpuSamplerRelease(tm->sampler);
    for (auto search = tm->texture_cache.begin(); search != tm->texture_cache.end(); search++) {
        wgpuTextureViewRelease(search->second.view);
        wgpuTextureRelease(search->second.texture);
    }
    tm->texture_cache.~unordered_map();
}
