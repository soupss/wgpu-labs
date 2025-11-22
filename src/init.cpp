#include "init.hpp"
#include <stdlib.h>
#include <stdio.h>
#include <webgpu.h>
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_wgpu.h>
#include "constants.h"
#include "util.hpp"
#include "model.h"

typedef struct AdapterRequest {
    WGPUAdapter *adapter;
    bool request_ended;
} AdapterRequest;

void _on_adapter_request_ended(
    WGPURequestAdapterStatus status,
    WGPUAdapter adapter,
    WGPUStringView message,
    void *userdata1,
    void *userdata2)
{
    AdapterRequest *state = (AdapterRequest*)userdata1;
    *(state->adapter) = adapter;
    state->request_ended = true;

    if (status == WGPURequestAdapterStatus_Success) {
        printf("Adapter request succeeded\n");
        // u_print_adapter_info(adapter);
    }
    else {
        fprintf(stderr,
                "Adapter request failed: %.*s\n",
                (int)message.length, message.data);
    }
}

typedef struct DeviceRequest {
    WGPUDevice *device;
    bool request_ended;
} DeviceRequest;

static void _on_device_request_ended(
    WGPURequestDeviceStatus status,
    WGPUDevice device,
    WGPUStringView message,
    void *userdata1,
    void *userdata2)
{
    DeviceRequest *device_request_state = (DeviceRequest*)userdata1;
    *(device_request_state->device) = device;
    device_request_state->request_ended = true;
    if (status == WGPURequestDeviceStatus_Success) {
        printf("Device request succeeded\n");
        // u_print_device_info(device);
    }
    else {
        fprintf(stderr,
                "Device request failed: %.*s\n",
                (int)message.length, message.data);
    }
}

static void _generate_mipmaps(WGPUDevice device, WGPUShaderModule module, WGPUSampler sampler, WGPUTexture texture, int mip_level_count) {
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

    typedef struct UniformSrcDim{
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

static void _get_mip_level_count(const int texture_width, const int texture_height, int *mip_level_count) {
    float max = fmaxf((float)texture_width, (float)texture_height);
    *mip_level_count = (int)floorf(log2f(max)) + 1;
}

// 1. Instance, adapter, device, queue
// 2. Surface
// 3. Shaders
// 4. Layout
// 5. Buffers
// 6. Textures
// 7. Bind groups
// 8. Pipeline
void initialize(State *s) {
    // ========================================
    // === INSTANCE, ADAPTER, DEVICE, QUEUE ===
    // ========================================

    SDL_Init(SDL_INIT_VIDEO);
    //TODO: fullscreen
    s->window = SDL_CreateWindow("a", WINDOW_WIDTH, WINDOW_HEIGHT, SDL_WINDOW_METAL);
    s->metal_view = SDL_Metal_CreateView(s->window);
    void* metal_layer = SDL_Metal_GetLayer(s->metal_view);

    WGPUInstanceDescriptor instance_desc = {
        .nextInChain= NULL
    };
    s->instance = wgpuCreateInstance(&instance_desc);

    WGPUSurfaceSourceMetalLayer source = {
        .chain = {
            .next = NULL,
            .sType = WGPUSType_SurfaceSourceMetalLayer
        },
        .layer = metal_layer
    };

    WGPUSurfaceDescriptor surface_desc = {
        .nextInChain = (const WGPUChainedStruct*)&source
    };
    s->surface = wgpuInstanceCreateSurface(s->instance, &surface_desc);

    AdapterRequest adapter_request_s = {
        .adapter = &(s->adapter),
        .request_ended = false
    };

    WGPURequestAdapterCallbackInfo adapter_callback_info = {
        .callback = _on_adapter_request_ended,
        .userdata1 = &adapter_request_s,
        .mode = WGPUCallbackMode_AllowProcessEvents
    };

    WGPURequestAdapterOptions adapter_request_options = {
        .nextInChain = NULL,
        .featureLevel = WGPUFeatureLevel_Core,
        .compatibleSurface = s->surface
    };

    wgpuInstanceRequestAdapter(s->instance, &adapter_request_options, adapter_callback_info);
    while(!adapter_request_s.request_ended) {
        wgpuInstanceProcessEvents(s->instance);
    }

    DeviceRequest device_request_state = {
        .device = &(s->device),
        .request_ended = false
    };

    WGPUDeviceDescriptor device_desc = {
        .nextInChain = NULL,
        .defaultQueue.nextInChain = NULL,
        .deviceLostCallbackInfo = {} // TODO: device lost callback
    };

    WGPURequestDeviceCallbackInfo device_callback_info = {
        .nextInChain = NULL,
        .callback = _on_device_request_ended,
        .userdata1 = &device_request_state,
        .mode  = WGPUCallbackMode_AllowProcessEvents
    };

    wgpuAdapterRequestDevice(s->adapter, &device_desc, device_callback_info);
    while (!device_request_state.request_ended) {
        wgpuInstanceProcessEvents(s->instance);
    }

    s->queue = wgpuDeviceGetQueue(s->device);

    // ===============
    // === SURFACE ===
    // ===============

    WGPUSurfaceConfiguration surface_config = {
        .nextInChain = NULL,
        .width = WINDOW_WIDTH,
        .height = WINDOW_HEIGHT,
        .viewFormatCount = 0,
        .viewFormats = NULL,
        .usage = WGPUTextureUsage_RenderAttachment,
        .device = s->device,
        .presentMode = WGPUPresentMode_Fifo,
        .alphaMode = WGPUCompositeAlphaMode_Auto
    };

    WGPUSurfaceCapabilities surface_capabilities = {};
    wgpuSurfaceGetCapabilities(s->surface, s->adapter, &surface_capabilities);
    WGPUTextureFormat surface_format = WGPUTextureFormat_Undefined;
    if (surface_capabilities.formatCount > 0) {
        surface_format = surface_capabilities.formats[0];
    }
    wgpuSurfaceCapabilitiesFreeMembers(surface_capabilities);

    surface_config.format = surface_format;
    wgpuSurfaceConfigure(s->surface, &surface_config);

    // ===============
    // === SHADERS ===
    // ===============

    int vertex_shader_words = 0;
    uint32_t *vertex_shader_source = NULL;;
    u_load_spirv(PATH_SHADER_VERTEX, &vertex_shader_source, &vertex_shader_words);
    WGPUShaderSourceSPIRV vertex_shader = {
        .chain.next = NULL,
        .chain.sType = WGPUSType_ShaderSourceSPIRV,
        .codeSize = (uint32_t)vertex_shader_words,
        .code = vertex_shader_source,
    };
    WGPUShaderModuleDescriptor vertex_shader_desc = {
        .nextInChain = &vertex_shader.chain
    };
    WGPUShaderModule vertex_shader_module = wgpuDeviceCreateShaderModule(s->device, &vertex_shader_desc);
    free(vertex_shader_source);

    int fragment_shader_words = 0;
    uint32_t *fragment_shader_source = NULL;
    u_load_spirv(PATH_SHADER_FRAGMENT, &fragment_shader_source, &fragment_shader_words);
    WGPUShaderSourceSPIRV fragment_shader = {
        .chain.next = NULL,
        .chain.sType = WGPUSType_ShaderSourceSPIRV,
        .codeSize = (uint32_t)fragment_shader_words,
        .code = fragment_shader_source
    };
    WGPUShaderModuleDescriptor fragment_shader_desc = {
        .nextInChain = &fragment_shader.chain
    };
    WGPUShaderModule fragment_shader_module = wgpuDeviceCreateShaderModule(s->device, &fragment_shader_desc);
    free(fragment_shader_source);

    int compute_shader_words = 0;
    uint32_t *compute_shader_source = NULL;
    u_load_spirv(PATH_SHADER_COMPUTE, &compute_shader_source, &compute_shader_words);
    WGPUShaderSourceSPIRV compute_shader = {
        .chain.next = NULL,
        .chain.sType = WGPUSType_ShaderSourceSPIRV,
        .codeSize = (uint32_t)compute_shader_words,
        .code = compute_shader_source
    };
    WGPUShaderModuleDescriptor compute_shader_desc = {
        .nextInChain = &compute_shader.chain
    };
    WGPUShaderModule compute_shader_module = wgpuDeviceCreateShaderModule(s->device, &compute_shader_desc);
    free(compute_shader_source);

    // ===============
    // === LAYOUTS ===
    // ===============

    WGPUBindGroupLayoutEntry bgl_entries[BG_ENTRY_COUNT] = {
        {
            .binding = 0,
            .visibility = WGPUShaderStage_Fragment | WGPUShaderStage_Vertex,
            .buffer.type = WGPUBufferBindingType_Uniform,
        },
        {
            .binding = 1,
            .visibility = WGPUShaderStage_Vertex,
            .buffer.type = WGPUBufferBindingType_Uniform,
            .buffer.hasDynamicOffset = true,
            .buffer.minBindingSize = UBO_OBJECT_SLOT_SIZE
        },
        {
            .binding = 2,
            .visibility = WGPUShaderStage_Fragment,
            .sampler.type = WGPUSamplerBindingType_Filtering
        }
    };

    WGPUBindGroupLayoutDescriptor bgl_desc = {
        .nextInChain = NULL,
        .entryCount = BG_ENTRY_COUNT,
        .entries = bgl_entries
    };
    WGPUBindGroupLayout bgl  = wgpuDeviceCreateBindGroupLayout(s->device, &bgl_desc);

    WGPUPipelineLayoutDescriptor pipeline_layout_desc = {
        .nextInChain = NULL,
        .bindGroupLayoutCount = 1,
        .bindGroupLayouts = &(bgl)
    };
    WGPUPipelineLayout pipeline_layout = wgpuDeviceCreatePipelineLayout(s->device, &pipeline_layout_desc);

    // ===============
    // === BUFFERS ===
    // ===============

    model_load(PATH_MODEL_CAR, &s->mesh_car);
    model_load(PATH_MODEL_CITY, &s->mesh_city);

    WGPUBufferDescriptor vbo_car_desc = {
        .nextInChain = NULL,
        .usage = WGPUBufferUsage_Vertex | WGPUBufferUsage_CopyDst,
        .size = s->mesh_car.vertex_count * VBO_STRIDE,
        .mappedAtCreation = false
    };
    s->vbo_car = wgpuDeviceCreateBuffer(s->device, &vbo_car_desc);

    wgpuQueueWriteBuffer(s->queue,
            s->vbo_car,
            0,
            s->mesh_car.vertices,
            s->mesh_car.vertex_count * VBO_STRIDE);

    WGPUBufferDescriptor vbo_city_desc = {
        .nextInChain = NULL,
        .usage = WGPUBufferUsage_Vertex | WGPUBufferUsage_CopyDst,
        .size = s->mesh_city.vertex_count * VBO_STRIDE,
        .mappedAtCreation = false
    };
    s->vbo_city = wgpuDeviceCreateBuffer(s->device, &vbo_city_desc);

    wgpuQueueWriteBuffer(s->queue,
            s->vbo_city,
            0,
            s->mesh_city.vertices,
            s->mesh_city.vertex_count * VBO_STRIDE);

    WGPUBufferDescriptor ibo_car_desc = {
        .nextInChain = NULL,
        .usage = WGPUBufferUsage_Index | WGPUBufferUsage_CopyDst,
        .size = s->mesh_car.index_count * sizeof(int),
        .mappedAtCreation = false
    };
    s->ibo_car = wgpuDeviceCreateBuffer(s->device, &ibo_car_desc);

    wgpuQueueWriteBuffer(s->queue,
            s->ibo_car,
            0,
            s->mesh_car.indices,
            s->mesh_car.index_count * sizeof(int));

    WGPUBufferDescriptor ibo_city_desc = {
        .nextInChain = NULL,
        .usage = WGPUBufferUsage_Index | WGPUBufferUsage_CopyDst,
        .size = s->mesh_city.index_count * sizeof(int),
        .mappedAtCreation = false
    };
    s->ibo_city = wgpuDeviceCreateBuffer(s->device, &ibo_city_desc);

    wgpuQueueWriteBuffer(s->queue,
            s->ibo_city,
            0,
            s->mesh_city.indices,
            s->mesh_city.index_count * sizeof(int));

    WGPUBufferDescriptor ubo_frame_desc = {
        .nextInChain = NULL,
        .usage = WGPUBufferUsage_Uniform | WGPUBufferUsage_CopyDst,
        .size = sizeof(UBOData_Frame),
        .mappedAtCreation = false
    };
    s->ubo_frame = wgpuDeviceCreateBuffer(s->device, &ubo_frame_desc);

    WGPUBufferDescriptor ubo_object_desc = {
        .nextInChain = NULL,
        .usage = WGPUBufferUsage_Uniform | WGPUBufferUsage_CopyDst,
        .size = UBO_OBJECT_SIZE,
        .mappedAtCreation = false
    };
    s->ubo_object = wgpuDeviceCreateBuffer(s->device, &ubo_object_desc);

    // ================
    // === TEXTURES ===
    // ================

    WGPUSamplerDescriptor sampler_desc = {
        .addressModeU = WGPUAddressMode_ClampToEdge,
        .addressModeV = WGPUAddressMode_Repeat,
        .addressModeW = WGPUAddressMode_ClampToEdge,
        .magFilter = WGPUFilterMode_Linear,
        .minFilter = WGPUFilterMode_Linear,
        .mipmapFilter = WGPUMipmapFilterMode_Linear,
        .lodMinClamp = 0.0f,
        .lodMaxClamp = 1000.0f,
        .compare = WGPUCompareFunction_Undefined,
        .maxAnisotropy = 16
    };

    WGPUSampler sampler = wgpuDeviceCreateSampler(s->device, &sampler_desc);

    // ===================
    // === BIND GROUPS ===
    // ===================

    WGPUBindGroupEntry bg_entries[BG_ENTRY_COUNT] = {
        {
            .binding = 0,
            .buffer = s->ubo_frame,
            .offset = 0,
            .size = sizeof(UBOData_Frame)
        },
        {
            .binding = 1,
            .buffer = s->ubo_object,
            .offset = 0,
            .size = UBO_OBJECT_SLOT_SIZE
        },
        {
            .binding = 2,
            .sampler = sampler
        }
    };

    WGPUBindGroupDescriptor bg = {
        .nextInChain = NULL,
        .layout = bgl,
        .entryCount = BG_ENTRY_COUNT,
        .entries = bg_entries
    };
    s->bg = wgpuDeviceCreateBindGroup(s->device, &bg);

    // ================
    // === PIPELINE ===
    // ================

    WGPUVertexAttribute vertex_attributes[VERTEX_ATTRIBUTE_COUNT] = {
        {
            .format = WGPUVertexFormat_Float32x3,
            .offset = 0,
            .shaderLocation = 0,
        },
        {
            .format = WGPUVertexFormat_Float32x3,
            .offset = 3 * sizeof(float),
            .shaderLocation = 1
        },
        {
            .format = WGPUVertexFormat_Float32x2,
            .offset = 6 * sizeof(float),
            .shaderLocation = 2
        }
    };

    WGPUVertexBufferLayout vbo_car_layout = {
        .stepMode = WGPUVertexStepMode_Vertex,
        .arrayStride = VBO_STRIDE,
        .attributeCount = VERTEX_ATTRIBUTE_COUNT,
        .attributes = vertex_attributes
    };

    WGPUBlendState blend_state = {
        .color.srcFactor = WGPUBlendFactor_SrcAlpha,
        .color.dstFactor = WGPUBlendFactor_OneMinusSrcAlpha,
        .color.operation = WGPUBlendOperation_Add,
        .alpha.srcFactor = WGPUBlendFactor_Zero,
        .alpha.dstFactor = WGPUBlendFactor_One,
        .alpha.operation = WGPUBlendOperation_Add
    };

    WGPUColorTargetState color_target = {
        .format = surface_format,
        .blend = &blend_state,
        .writeMask = WGPUColorWriteMask_All
    };

    WGPUFragmentState fragment_state = {
        .module = fragment_shader_module,
        .entryPoint = {
            .data = "main",
            .length = WGPU_STRLEN
        },
        .constantCount = 0,
        .constants = NULL,
        .targetCount = 1,
        .targets = &color_target,
    };

    WGPURenderPipelineDescriptor pipeline_desc = {
        .nextInChain = NULL,

        .vertex.entryPoint = {
            .data = "main",
            .length = WGPU_STRLEN
        },
        .vertex.constantCount = 0,
        .vertex.constants = NULL,
        .vertex.bufferCount = 1,
        .vertex.buffers = &vbo_car_layout,
        .vertex.module = vertex_shader_module,

        .primitive.topology = WGPUPrimitiveTopology_TriangleList,
        .primitive.stripIndexFormat = WGPUIndexFormat_Undefined,
        .primitive.frontFace = WGPUFrontFace_CCW,
        .primitive.cullMode = WGPUCullMode_Front,

        .fragment = &fragment_state,

        .layout = pipeline_layout,
        .depthStencil = NULL,
        .multisample.count = 1,
        .multisample.mask = ~0u,
        .multisample.alphaToCoverageEnabled = false,
    };
    s->pipeline = wgpuDeviceCreateRenderPipeline(s->device, &pipeline_desc);
    wgpuPipelineLayoutRelease(pipeline_layout);

    ImGui::CreateContext();
    ImGui_ImplSDL3_InitForMetal(s->window);
    ImGui_ImplWGPU_InitInfo imgui_init = {};
    imgui_init.Device = s->device;
    imgui_init.NumFramesInFlight = 3;
    imgui_init.RenderTargetFormat = surface_format;
    imgui_init.DepthStencilFormat = WGPUTextureFormat_Undefined;
    ImGui_ImplWGPU_Init(&imgui_init);

    wgpuShaderModuleRelease(vertex_shader_module);
    wgpuShaderModuleRelease(fragment_shader_module);
}
