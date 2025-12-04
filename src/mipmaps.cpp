#include "mipmaps.h"
#include <cglm/cglm.h>
#include "constants.h"

int mipmaps_get_count(const int texture_width, const int texture_height) {
    float max = fmaxf((float)texture_width, (float)texture_height);
    if (max == 1) return 1;
    return (int)floorf(log2f(max)) + 1;
}

void mipmaps_generate(WGPUDevice device, WGPUShaderModule module, WGPUSampler sampler, WGPUTexture texture, int mip_level_count) {
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

    WGPUBindGroupLayout bgl = wgpuDeviceCreateBindGroupLayout(device, &bgl_desc);

    WGPUPipelineLayoutDescriptor comp_pipeline_layout_desc = {
        .nextInChain = NULL,
        .bindGroupLayoutCount = 1,
        .bindGroupLayouts = &bgl
    };

    WGPUPipelineLayout comp_pipeline_layout = wgpuDeviceCreatePipelineLayout(device, &comp_pipeline_layout_desc);

    WGPUComputePipelineDescriptor comp_pipeline_desc = {
        .compute.module = module,
        .compute.entryPoint = {
            .data = "main",
            .length = WGPU_STRLEN
        },
        .layout = comp_pipeline_layout
    };

    WGPUComputePipeline comp_pipeline = wgpuDeviceCreateComputePipeline(device, &comp_pipeline_desc);

    WGPUQueue queue = wgpuDeviceGetQueue(device);

    typedef struct {
        int texview_src_width;
        int texview_src_height;
    } UniformSrcDim;

    WGPUBufferDescriptor buffer_uniforms_desc = {
        .usage = WGPUBufferUsage_Uniform | WGPUBufferUsage_CopyDst,
        .size = sizeof(UniformSrcDim),
        .mappedAtCreation = false
    };

    WGPUBuffer buffer_uniforms = wgpuDeviceCreateBuffer(device, &buffer_uniforms_desc);

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
                .sampler = sampler,
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

        WGPUBindGroup bg = wgpuDeviceCreateBindGroup(device, &bg_desc);

        WGPUCommandEncoder encoder = wgpuDeviceCreateCommandEncoder(device, NULL);
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
