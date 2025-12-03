#include "util.h"
#include "webgpu.h"
#include <stdio.h>
#include <string.h>
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

static void _texturemap_fallback(const WGPUDevice device, const WGPUQueue queue, TextureMap *tm) {
    WGPUTextureDescriptor tex_desc = {
        .label = {"texturemap fallback",  WGPU_STRLEN},
        .size = {1, 1, 1},
        .mipLevelCount = 1,
        .sampleCount = 1,
        .dimension = WGPUTextureDimension_2D,
        .format = WGPUTextureFormat_RGBA8UnormSrgb,
        .usage = WGPUTextureUsage_TextureBinding | WGPUTextureUsage_CopyDst,
    };

    tm->texture = wgpuDeviceCreateTexture(device, &tex_desc);

    WGPUTexelCopyTextureInfo dest = {
        .texture = tm->texture,
        .mipLevel = 0,
        .origin = {0, 0, 0},
        .aspect = WGPUTextureAspect_All
    };

    WGPUTexelCopyBufferLayout data_layout = {
        .offset = 0,
        .bytesPerRow = 4* sizeof(char),
        .rowsPerImage = 1
    };

    WGPUExtent3D write_size = {
        .width = 1,
        .height = 1,
        .depthOrArrayLayers = 1
    };

    unsigned char pixel[4] = {255, 255, 255, 255};

    wgpuQueueWriteTexture(queue, &dest, pixel, 4 * sizeof(char), &data_layout, &write_size);

    tm->view = wgpuTextureCreateView(tm->texture, NULL);
}

void u_texturemap_load(const WGPUDevice device, const WGPUQueue queue, TextureMap *tm, const char *path) {
    if (!path || strlen(path) == 0) {
        _texturemap_fallback(device, queue, tm);
        return;
    }

    int w,h;
    const int channels = 4;
    stbi_set_flip_vertically_on_load(true);
    unsigned char *pixels = stbi_load(path, &w, &h, NULL, channels);
    if (!pixels) {
        fprintf(stderr, "Failed to load image %s: %s!\n", path, stbi_failure_reason());
        return;
    }

    WGPUTextureDescriptor tex_desc = {
        .label = {path,  WGPU_STRLEN},
        .size = {(uint32_t)w, (uint32_t)h, 1},
        .mipLevelCount = 1,
        .sampleCount = 1,
        .dimension = WGPUTextureDimension_2D,
        .format = WGPUTextureFormat_RGBA8UnormSrgb,
        .usage = WGPUTextureUsage_TextureBinding | WGPUTextureUsage_CopyDst,
    };

    tm->texture = wgpuDeviceCreateTexture(device, &tex_desc);

    WGPUTexelCopyTextureInfo dest = {
        .texture = tm->texture,
        .mipLevel = 0,
        .origin = {0, 0, 0},
        .aspect = WGPUTextureAspect_All
    };

    WGPUTexelCopyBufferLayout data_layout = {
        .offset = 0,
        .bytesPerRow = (uint32_t)(w * 4 * sizeof(unsigned char)),
        .rowsPerImage = (uint32_t)h
    };

    WGPUExtent3D write_size = {
        .width = (uint32_t)w,
        .height = (uint32_t)h,
        .depthOrArrayLayers = 1
    };

    wgpuQueueWriteTexture(queue, &dest, pixels, w * h * channels * sizeof(unsigned char), &data_layout, &write_size);

    stbi_image_free(pixels);

    WGPUTextureViewDescriptor view_desc = {
        .baseMipLevel = 0,
        .mipLevelCount = 1,
        .baseArrayLayer = 0,
        .arrayLayerCount = 1,
        .format = WGPUTextureFormat_RGBA8UnormSrgb,
        .dimension = WGPUTextureViewDimension_2D,
        .aspect = WGPUTextureAspect_All,
        .usage = WGPUTextureUsage_TextureBinding | WGPUTextureUsage_CopyDst,
    };
    tm->view = wgpuTextureCreateView(tm->texture, &view_desc);
}

void u_get_texture_path(char *out, size_t cap, const char *dir, const char *filename) {
    if (!out) return;
    if (!dir || !filename) {
        out[0] = '\0';
        return;
    }

    // 1. Find the last slash or backslash in filename
    const char *name = strrchr(filename, '\\');
    if (!name) name = strrchr(filename, '/');

    // 2. Move past the slash if found
    if (name)
        name++;
    else
        name = filename;

    // 3. Join dir + name with exactly one slash
    size_t dir_len = strlen(dir);
    if (dir_len > 0 && dir[dir_len - 1] == '/')
        snprintf(out, cap, "%s%s", dir, name);
    else
        snprintf(out, cap, "%s/%s", dir, name);
}

void u_load_spirv(const char* path, uint32_t** out_data, int* out_word_count) {
    *out_data = NULL;
    if (out_word_count) {
        *out_word_count = 0;
    }
    FILE* f = fopen(path, "rb");
    if (!f) {
        fprintf(stderr, "Failed to open %s\n", path);
        return;
    }
    if (fseek(f, 0, SEEK_END) != 0) {
        fprintf(stderr, "Failed to seek %s\n", path);
        fclose(f);
        return;
    }
    long size = ftell(f);
    if (size < 0 || size % 4 != 0) {
        fprintf(stderr, "Bad SPIR-V file size for %s\n", path);
        fclose(f);
        return;
    }
    if (fseek(f, 0, SEEK_SET) != 0) {
        fprintf(stderr, "Failed to rewind %s\n", path);
        fclose(f);
        return;
    }
    uint32_t* data = (uint32_t*)malloc((size_t)size);
    if (!data) {
        fprintf(stderr, "Out of memory reading %s\n", path);
        fclose(f);
        return;
    }
    if (fread(data, 1, (size_t)size, f) != (size_t)size) {
        fprintf(stderr, "Failed to read %s\n", path);
        free(data);
        fclose(f);
        return;
    }
    fclose(f);
    *out_data = data;
    if (out_word_count) {
        *out_word_count = (size_t)size / 4;
    }
}

void u_print_adapter_info(WGPUAdapter adapter) {
    // Adapter info
    WGPUAdapterInfo info = {};
    if (wgpuAdapterGetInfo(adapter, &info) == WGPUStatus_Success) {
        printf("=== Adapter Info ===\n");
        printf("Vendor:       %.*s\n", (int)info.vendor.length, info.vendor.data);
        printf("Architecture: %.*s\n", (int)info.architecture.length, info.architecture.data);
        printf("Device:       %.*s\n", (int)info.device.length, info.device.data);
        printf("Description:  %.*s\n", (int)info.description.length, info.description.data);
        printf("Backend:      %d\n", info.backendType);
        printf("Adapter Type: %d\n", info.adapterType);
        printf("Vendor ID:    %u\n", info.vendorID);
        printf("Device ID:    %u\n", info.deviceID);
    }
    wgpuAdapterInfoFreeMembers(info);

    // Adapter features
    WGPUSupportedFeatures features = {};
    wgpuAdapterGetFeatures(adapter, &features);
    printf("\n=== Supported Features (%zu) ===\n", features.featureCount);
    for (size_t i = 0; i < features.featureCount; ++i) {
        printf("  - Feature #%zu: %d\n", i, features.features[i]);
    }
    wgpuSupportedFeaturesFreeMembers(features);

    // Adapter limits
    WGPULimits limits = {};
    if (wgpuAdapterGetLimits(adapter, &limits) == WGPUStatus_Success) {
        printf("\n=== Adapter Limits ===\n");
        printf("Max 2D Texture Dimension: %u\n", limits.maxTextureDimension2D);
        printf("Max Bind Groups:          %u\n", limits.maxBindGroups);
        printf("Max Uniform Buffer Size:  %llu\n", (unsigned long long)limits.maxUniformBufferBindingSize);
        printf("Max Storage Buffer Size:  %llu\n", (unsigned long long)limits.maxStorageBufferBindingSize);
        printf("Max Vertex Buffers:       %u\n", limits.maxVertexBuffers);
        printf("Max Vertex Attributes:    %u\n", limits.maxVertexAttributes);
        printf("Max Compute Workgroup X:  %u\n", limits.maxComputeWorkgroupSizeX);
        printf("Max Compute Workgroup Y:  %u\n", limits.maxComputeWorkgroupSizeY);
        printf("Max Compute Workgroup Z:  %u\n", limits.maxComputeWorkgroupSizeZ);
    }
    printf("========================\n");
}

void u_print_device_info(WGPUDevice device) {
    // --- Device features
    WGPUSupportedFeatures dfeatures = (WGPUSupportedFeatures){};
    wgpuDeviceGetFeatures(device, &dfeatures);
    printf("\n=== Device Features (%zu) ===\n", dfeatures.featureCount);
    for (size_t i = 0; i < dfeatures.featureCount; ++i) {
        printf("  - %d\n", dfeatures.features[i]); // WGPUFeatureName
    }
    wgpuSupportedFeaturesFreeMembers(dfeatures);

    // --- Device limits
    WGPULimits dlimits = (WGPULimits){};
    if (wgpuDeviceGetLimits(device, &dlimits) == WGPUStatus_Success) {
        printf("\n=== Device Limits ===\n");
        printf("Max 2D Texture Dimension: %u\n", dlimits.maxTextureDimension2D);
        printf("Max Bind Groups:          %u\n", dlimits.maxBindGroups);
        printf("Max Uniform Buffer Size:  %llu\n", (unsigned long long)dlimits.maxUniformBufferBindingSize);
        printf("Max Storage Buffer Size:  %llu\n", (unsigned long long)dlimits.maxStorageBufferBindingSize);
        printf("Max Vertex Buffers:       %u\n", dlimits.maxVertexBuffers);
        printf("Max Vertex Attributes:    %u\n", dlimits.maxVertexAttributes);
        printf("Max Compute WG Size X/Y/Z:%u/%u/%u\n",
               dlimits.maxComputeWorkgroupSizeX,
               dlimits.maxComputeWorkgroupSizeY,
               dlimits.maxComputeWorkgroupSizeZ);
    }
    printf("========================\n");
}
