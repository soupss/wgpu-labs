#include "texture_manager.h"
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#include "state.h"
#include "constants.h"

static int _texture_manager_mipmaps_get_count(const int texture_width, const int texture_height) {
    float max = fmaxf((float)texture_width, (float)texture_height);
    if (max == 1) return 1;
    return (int)floorf(log2f(max)) + 1;
}

static void _texture_manager_mipmaps_generate(const TextureManager *tm, const WGPUShaderModule module, const WGPUTexture texture, const int mip_level_count) {
    const int tex_width = wgpuTextureGetWidth(texture);
    const int tex_height = wgpuTextureGetHeight(texture);

    WGPUBindGroupLayoutEntry bgl_entries[BG_COMP_ENTRY_COUNT] = {
        {
            .binding = 0,
            .visibility = WGPUShaderStage_Compute,
            .sampler.type = WGPUSamplerBindingType_Filtering
        },
        {
            .binding = 1,
            .visibility = WGPUShaderStage_Compute,
            .texture = {
                .sampleType = WGPUTextureSampleType_Float,
                .viewDimension = WGPUTextureViewDimension_2D,
            }
        },
        {
            .binding = 2,
            .visibility = WGPUShaderStage_Compute,
            .storageTexture = {
                .access = WGPUStorageTextureAccess_WriteOnly,
                .viewDimension = WGPUTextureViewDimension_2D,
                .format = WGPUTextureFormat_RGBA8Unorm,
            }
        },
        {
            .binding = 3,
            .visibility = WGPUShaderStage_Compute,
            .buffer.type = WGPUBufferBindingType_Uniform,
        }
    };

    WGPUBindGroupLayoutDescriptor bgl_desc = {
        .entryCount = BG_COMP_ENTRY_COUNT,
        .entries = bgl_entries
    };

    WGPUBindGroupLayout bgl = wgpuDeviceCreateBindGroupLayout(tm->device, &bgl_desc);

    WGPUPipelineLayoutDescriptor comp_pipeline_layout_desc = {
        .nextInChain = NULL,
        .bindGroupLayoutCount = 1,
        .bindGroupLayouts = &bgl
    };

    WGPUPipelineLayout comp_pipeline_layout = wgpuDeviceCreatePipelineLayout(tm->device, &comp_pipeline_layout_desc);

    WGPUComputePipelineDescriptor comp_pipeline_desc = {
        .compute.module = module,
        .compute.entryPoint = {
            .data = "main",
            .length = WGPU_STRLEN
        },
        .layout = comp_pipeline_layout
    };

    WGPUComputePipeline comp_pipeline = wgpuDeviceCreateComputePipeline(tm->device, &comp_pipeline_desc);

    WGPUQueue queue = wgpuDeviceGetQueue(tm->device);

    typedef struct {
        int texview_src_width;
        int texview_src_height;
    } UniformSrcDim;

    WGPUBufferDescriptor buffer_uniforms_desc = {
        .usage = WGPUBufferUsage_Uniform | WGPUBufferUsage_CopyDst,
        .size = sizeof(UniformSrcDim),
        .mappedAtCreation = false
    };

    WGPUBuffer buffer_uniforms = wgpuDeviceCreateBuffer(tm->device, &buffer_uniforms_desc);

    for (int mip = 0; mip < mip_level_count - 1; mip++) {
        WGPUTextureViewDescriptor texview_src_desc = {
            .dimension = WGPUTextureViewDimension_2D,
            .baseMipLevel = (uint32_t)mip,
            .mipLevelCount = 1,
            .baseArrayLayer = 0,
            .arrayLayerCount = 1,
            .aspect = WGPUTextureAspect_All
        };
        WGPUTextureView texview_src = wgpuTextureCreateView(texture, &texview_src_desc);

        WGPUTextureViewDescriptor texview_dst_desc = texview_src_desc;
        texview_dst_desc.baseMipLevel = mip + 1;
        WGPUTextureView texview_dst = wgpuTextureCreateView(texture, &texview_dst_desc);

        UniformSrcDim src_dim = {
            .texview_src_width = tex_width >> (mip),
            .texview_src_height = tex_height >> (mip)
        };

        wgpuQueueWriteBuffer(queue, buffer_uniforms, 0, &src_dim, sizeof(UniformSrcDim));

        WGPUBindGroupEntry bg_entries[BG_COMP_ENTRY_COUNT] = {
            {
                .binding = 0,
                .sampler = tm->sampler,
            },
            {
                .binding = 1,
                .textureView = texview_src,
            },
            {
                .binding = 2,
                .textureView = texview_dst,
            },
            {
                .binding = 3,
                .buffer = buffer_uniforms,
                .offset = 0,
                .size = sizeof(UniformSrcDim)
            }
        };

        WGPUBindGroupDescriptor bg_desc = {
            .nextInChain = NULL,
            .layout = bgl,
            .entryCount = BG_COMP_ENTRY_COUNT,
            .entries = bg_entries
        };

        WGPUBindGroup bg = wgpuDeviceCreateBindGroup(tm->device, &bg_desc);

        WGPUCommandEncoder encoder = wgpuDeviceCreateCommandEncoder(tm->device, NULL);
        WGPUComputePassEncoder pass = wgpuCommandEncoderBeginComputePass(encoder, NULL);

        wgpuComputePassEncoderSetPipeline(pass, comp_pipeline);
        wgpuComputePassEncoderSetBindGroup(pass, 0, bg, 0, NULL);

        int texview_dst_width = tex_width >> (mip+1);
        int texview_dst_height = tex_height >> (mip+1);

        wgpuComputePassEncoderDispatchWorkgroups(pass, texview_dst_width, texview_dst_height, 1);

        wgpuComputePassEncoderEnd(pass);

        WGPUCommandBuffer command_buffer = wgpuCommandEncoderFinish(encoder, NULL);
        wgpuCommandEncoderRelease(encoder);

        wgpuQueueSubmit(queue, 1, &command_buffer);
        wgpuCommandBufferRelease(command_buffer);
        wgpuTextureViewRelease(texview_src);
        wgpuTextureViewRelease(texview_dst);
    }

    wgpuPipelineLayoutRelease(comp_pipeline_layout);
    wgpuBufferRelease(buffer_uniforms);
    wgpuBindGroupLayoutRelease(bgl);
    wgpuComputePipelineRelease(comp_pipeline);
    wgpuQueueRelease(queue);
}

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
