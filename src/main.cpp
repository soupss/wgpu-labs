#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <webgpu.h>
#include <SDL3/SDL.h>
#include <cglm/cglm.h>
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_wgpu.h>
#include "util.hpp"

#define PATH_SHADER_VERTEX "build/vertex.spv"
#define PATH_SHADER_FRAGMENT "build/fragment.spv"
#define PATH_SHADER_COMPUTE "build/compute.spv"
#define PATH_TEXTURE_ASPHALT "assets/textures/asphalt.jpg"
#define PATH_TEXTURE_EXPLOSION "assets/textures/explosion.png"

#define WINDOW_WIDTH 1200
#define WINDOW_HEIGHT 800

#define VERTEX_COUNT_QUAD 4
#define VERTEX_COUNT_TOTAL (2 * VERTEX_COUNT_QUAD)
#define VERTEX_BUFFER_STRIDE 5 // 3 pos + 2 uv

#define INDEX_COUNT_QUAD 6
#define INDEX_COUNT_TOTAL (2 * INDEX_COUNT_QUAD)

#define BG_ENTRY_COUNT 3

typedef struct State {
    SDL_Window *window;
    SDL_MetalView metal_view;
    WGPUAdapter adapter;
    WGPUDevice device;
    WGPUInstance instance;
    WGPUSurface surface;
    WGPURenderPipeline pipeline;
    WGPUQueue queue;
    WGPUBuffer uniform_buffer;
    WGPUBuffer vertex_buffer;
    WGPUBuffer index_buffer;
    size_t vertex_buffer_size;
    WGPUTexture texture_asphalt;
    WGPUTexture texture_explosion;
    WGPUBindGroup bg_asphalt;
    WGPUBindGroup bg_explosion;
} State;

typedef struct Imgui {
    float camera_pan;
    float anisotropy;
} Imgui;

typedef struct Uniforms {
    float time;
    mat4 projection_matrix;
    vec3 camera_position;
} Uniforms;

typedef struct AdapterRequest {
    WGPUAdapter *adapter;
    bool request_ended;
} AdapterRequest;

void on_adapter_request_ended(
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
        // print_adapter_info(adapter);
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

void on_device_request_ended(
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
        // print_device_info(device);
    }
    else {
        fprintf(stderr,
                "Device request failed: %.*s\n",
                (int)message.length, message.data);
    }
}

void render_imgui(Imgui *imgui) {
    ImGui_ImplSDL3_NewFrame();
    ImGui_ImplWGPU_NewFrame();
    ImGui::NewFrame();

    ImGui::SliderFloat("Anisotropic filtering", &(imgui->anisotropy), 1.0, 16.0, "Number of samples: %.0f");
	ImGui::Dummy({ 0, 20 });
	ImGui::SliderFloat("Camera Panning", &(imgui->camera_pan), -1.0, 1.0);


    ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);

    ImGui::Render();
}

void get_mip_level_count(const int texture_width, const int texture_height, int *mip_level_count) {
    float max = fmaxf((float)texture_width, (float)texture_height);
    *mip_level_count = (int)floorf(log2f(max)) + 1;
}

typedef struct MapBufferRequest {
    bool request_ended;
} MapBufferRequest;

void on_staging_buffer_mapped(
        WGPUMapAsyncStatus status,
        WGPUStringView message,
        void *userdata1,
        void *userdata2
        )
{
    MapBufferRequest *request_state = (MapBufferRequest*)userdata1;
    request_state->request_ended = true;

}

void square_array(WGPUDevice device, WGPUShaderModule module) {
    ////////////
    // LAYOUT //
    ////////////

    WGPUBindGroupLayoutEntry bgl_entry = {
        .binding = 0,
        .visibility = WGPUShaderStage_Compute,
        .buffer.type = WGPUBufferBindingType_Storage
    };

    WGPUBindGroupLayoutDescriptor bgl_desc = {
        .entryCount = 1,
        .entries = &bgl_entry
    };

    WGPUBindGroupLayout bgl = wgpuDeviceCreateBindGroupLayout(device, &bgl_desc);

    WGPUPipelineLayoutDescriptor comp_pipeline_layout_desc = {
        .nextInChain = NULL,
        .bindGroupLayoutCount = 1,
        .bindGroupLayouts = &bgl
    };
    WGPUPipelineLayout comp_pipeline_layout = wgpuDeviceCreatePipelineLayout(device, &comp_pipeline_layout_desc);

    /////////////
    // BUFFERS //
    /////////////

    float in[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};

    WGPUBufferDescriptor buffer_storage_desc = {
        .usage = WGPUBufferUsage_Storage | WGPUBufferUsage_CopyDst | WGPUBufferUsage_CopySrc,
        .size = sizeof(in)
    };
    WGPUBuffer buffer_storage = wgpuDeviceCreateBuffer(device, &buffer_storage_desc);

    WGPUBufferDescriptor buffer_stage_desc = {
        .usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_MapRead,
        .size = sizeof(in)
    };
    WGPUBuffer buffer_stage = wgpuDeviceCreateBuffer(device, &buffer_stage_desc);

    WGPUQueue queue = wgpuDeviceGetQueue(device);
    wgpuQueueWriteBuffer(queue, buffer_storage, 0, in, sizeof(in));


    /////////////////
    // BIND GROUPS //
    /////////////////

    WGPUBindGroupEntry bg_entry = {
        .binding = 0,
        .buffer = buffer_storage,
        .offset = 0,
        .size = sizeof(in)
    };

    WGPUBindGroupDescriptor bg_desc = {
        .nextInChain = NULL,
        .layout = bgl,
        .entryCount = 1,
        .entries = &bg_entry
    };

    WGPUBindGroup bg = wgpuDeviceCreateBindGroup(device, &bg_desc);

    wgpuBindGroupLayoutRelease(bgl);

    //////////////
    // PIPELINE //
    //////////////

    WGPUComputePipelineDescriptor comp_pipeline_desc = {
        .compute.module = module,
        .compute.entryPoint = {
            .data = "main",
            .length = WGPU_STRLEN
        },
        .layout = comp_pipeline_layout
    };

    WGPUComputePipeline comp_pipeline = wgpuDeviceCreateComputePipeline(device, &comp_pipeline_desc);

    WGPUCommandEncoder encoder = wgpuDeviceCreateCommandEncoder(device, NULL);

    //////////
    // PASS //
    //////////

    WGPUComputePassEncoder pass = wgpuCommandEncoderBeginComputePass(encoder, NULL);

    wgpuComputePassEncoderSetPipeline(pass, comp_pipeline);
    wgpuComputePassEncoderSetBindGroup(pass, 0, bg, 0, NULL);

    wgpuComputePassEncoderDispatchWorkgroups(pass, 10, 1, 1);

    wgpuComputePassEncoderEnd(pass);

    wgpuCommandEncoderCopyBufferToBuffer(encoder, buffer_storage, 0, buffer_stage, 0, sizeof(in));

    WGPUCommandBuffer command_buffer = wgpuCommandEncoderFinish(encoder, NULL);
    wgpuCommandEncoderRelease(encoder);

    wgpuQueueSubmit(queue, 1, &command_buffer);

    wgpuCommandBufferRelease(command_buffer);
    wgpuQueueRelease(queue);

    //////////////
    // COPYBACK //
    //////////////

    MapBufferRequest request = {
        .request_ended = false
    };

    WGPUBufferMapCallbackInfo callback_info = {
        .nextInChain = NULL,
        .mode = WGPUCallbackMode_AllowProcessEvents,
        .callback = on_staging_buffer_mapped,
        .userdata1 = &request,
    };

    wgpuBufferMapAsync(buffer_stage, WGPUMapMode_Read, 0, sizeof(in), callback_info);
    while (!request.request_ended) {
        wgpuDevicePoll(device, true, NULL);
    }

    const float *out = (const float*)wgpuBufferGetConstMappedRange(buffer_stage, 0, sizeof(in));

    for (int i = 0; i < 10; i++) {
        printf("%f\n", out[i]);
    }

    wgpuBufferUnmap(buffer_stage);
}

// 1. Instance, adapter, device, queue
// 2. Surface
// 3. Shaders
// 4. Layout
// 5. Buffers
// 6. Textures
// 7. Bind groups
// 8. Pipeline
void initialize(State *s)
{
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
        .callback = on_adapter_request_ended,
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
        .callback = on_device_request_ended,
        .userdata1 = &device_request_state,
        .mode  = WGPUCallbackMode_AllowProcessEvents
    };

    wgpuAdapterRequestDevice(s->adapter, &device_desc, device_callback_info);
    while (!device_request_state.request_ended) { // TODO: prettier busywait
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
    load_spirv(PATH_SHADER_VERTEX, &vertex_shader_source, &vertex_shader_words);
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
    load_spirv(PATH_SHADER_FRAGMENT, &fragment_shader_source, &fragment_shader_words);
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
    load_spirv(PATH_SHADER_COMPUTE, &compute_shader_source, &compute_shader_words);
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
            .buffer.type = WGPUBufferBindingType_Uniform
        },
        {
            .binding = 1,
            .visibility = WGPUShaderStage_Fragment,
            .sampler.type = WGPUSamplerBindingType_Filtering
        },
        {
            .binding = 2,
            .visibility = WGPUShaderStage_Fragment,
            .texture.sampleType = WGPUTextureSampleType_Float,
            .texture.viewDimension = WGPUTextureViewDimension_2D
        }
    };

    WGPUBindGroupLayoutDescriptor bgl_desc = {
        .nextInChain = NULL,
        .entryCount = BG_ENTRY_COUNT,
        .entries = bgl_entries
    };
    WGPUBindGroupLayout bgl = wgpuDeviceCreateBindGroupLayout(s->device, &bgl_desc);

    WGPUPipelineLayoutDescriptor pipeline_layout_desc = {
        .nextInChain = NULL,
        .bindGroupLayoutCount = 1,
        .bindGroupLayouts = &bgl
    };
    WGPUPipelineLayout pipeline_layout = wgpuDeviceCreatePipelineLayout(s->device, &pipeline_layout_desc);

    // ===============
    // === BUFFERS ===
    // ===============

    WGPUBufferDescriptor vertex_buffer_desc = {
        .nextInChain = NULL,
        .usage = WGPUBufferUsage_Vertex | WGPUBufferUsage_CopyDst,
        .size = VERTEX_COUNT_TOTAL * VERTEX_BUFFER_STRIDE * sizeof(float),
        .mappedAtCreation = false
    };
    s->vertex_buffer = wgpuDeviceCreateBuffer(s->device, &vertex_buffer_desc);

    WGPUBufferDescriptor index_buffer_desc = {
        .nextInChain = NULL,
        .usage = WGPUBufferUsage_Index | WGPUBufferUsage_CopyDst,
        .size = INDEX_COUNT_TOTAL * sizeof(int),
        .mappedAtCreation = false
    };
    s->index_buffer = wgpuDeviceCreateBuffer(s->device, &index_buffer_desc);

    WGPUBufferDescriptor uniform_buffer_desc = {
        .nextInChain = NULL,
        .usage = WGPUBufferUsage_Uniform | WGPUBufferUsage_CopyDst,
        .size = sizeof(Uniforms),
        .mappedAtCreation = false
    };
    s->uniform_buffer = wgpuDeviceCreateBuffer(s->device, &uniform_buffer_desc);

    // ================
    // === TEXTURES ===
    // ================


    // asphalt texture

    int image_asphalt_width, image_asphalt_height;
    const int IMAGE_ASPHALT_CHANNELS = 4;
    unsigned char *image_asphalt = stbi_load(PATH_TEXTURE_ASPHALT, &image_asphalt_width, &image_asphalt_height, NULL, IMAGE_ASPHALT_CHANNELS);

    //TODO: texture->tex
    int texture_asphalt_mip_level_count;
    get_mip_level_count(image_asphalt_width, image_asphalt_height, &texture_asphalt_mip_level_count);

    WGPUTextureDescriptor texture_asphalt_desc = {
        .size = {(uint32_t)image_asphalt_width, (uint32_t)image_asphalt_height, 1},
        .nextInChain = NULL,
        .mipLevelCount = (uint32_t)texture_asphalt_mip_level_count,
        .sampleCount = 1,
        .format = WGPUTextureFormat_RGBA8Unorm,
        .usage = WGPUTextureUsage_CopyDst | WGPUTextureUsage_TextureBinding,
        .viewFormatCount = 0,
        .viewFormats = NULL

    };
    s->texture_asphalt = wgpuDeviceCreateTexture(s->device, &texture_asphalt_desc);

    square_array(s->device, compute_shader_module);

    WGPUTexelCopyTextureInfo texture_asphalt_destination = {
        .texture = s->texture_asphalt,
        .mipLevel = 0,
        .origin = {0, 0, 0},
        .aspect = WGPUTextureAspect_All
    };

    WGPUTexelCopyBufferLayout texture_asphalt_layout = {
        .offset = 0,
        .bytesPerRow = IMAGE_ASPHALT_CHANNELS * texture_asphalt_desc.size.width,
        .rowsPerImage = texture_asphalt_desc.size.height
    };

    wgpuQueueWriteTexture(
        s->queue,
        &texture_asphalt_destination,
        image_asphalt,
        IMAGE_ASPHALT_CHANNELS * texture_asphalt_desc.size.width * texture_asphalt_desc.size.height,
        &texture_asphalt_layout,
        &texture_asphalt_desc.size
    );

    stbi_image_free(image_asphalt);

    WGPUTextureViewDescriptor texture_view_asphalt_desc = {
        .aspect = WGPUTextureAspect_All,
        .baseArrayLayer = 0,
        .arrayLayerCount = 1,
        .baseMipLevel = 0,
        .mipLevelCount = 1,
        .dimension = WGPUTextureViewDimension_2D,
        .format = texture_asphalt_desc.format
    };
    WGPUTextureView texture_view_asphalt = wgpuTextureCreateView(s->texture_asphalt, &texture_view_asphalt_desc);

    // explosion texture

    int image_explosion_width, image_explosion_height;
    const int IMAGE_EXPLOSION_CHANNELS = 4;
    unsigned char *image_explosion = stbi_load(PATH_TEXTURE_EXPLOSION, &image_explosion_width, &image_explosion_height, NULL, IMAGE_EXPLOSION_CHANNELS);

    WGPUTextureDescriptor texture_explosion_desc = {
        .size = {(uint32_t)image_explosion_width, (uint32_t)image_explosion_height, 1},
        .nextInChain = NULL,
        .mipLevelCount = 1,
        .sampleCount = 1,
        .format = WGPUTextureFormat_RGBA8Unorm,
        .usage = WGPUTextureUsage_CopyDst | WGPUTextureUsage_TextureBinding,
        .viewFormatCount = 0,
        .viewFormats = NULL
    };
    s->texture_explosion = wgpuDeviceCreateTexture(s->device, &texture_explosion_desc);

    WGPUTexelCopyTextureInfo texture_explosion_destination = {
        .texture = s->texture_explosion,
        .mipLevel = 0,
        .origin = {0, 0, 0},
        .aspect = WGPUTextureAspect_All
    };

    WGPUTexelCopyBufferLayout texture_explosion_layout = {
        .offset = 0,
        .bytesPerRow = IMAGE_EXPLOSION_CHANNELS * texture_explosion_desc.size.width,
        .rowsPerImage = texture_explosion_desc.size.height
    };

    wgpuQueueWriteTexture(
        s->queue,
        &texture_explosion_destination,
        image_explosion,
        IMAGE_EXPLOSION_CHANNELS * texture_explosion_desc.size.width * texture_explosion_desc.size.height,
        &texture_explosion_layout,
        &texture_explosion_desc.size
    );

    stbi_image_free(image_explosion);

    WGPUTextureViewDescriptor texture_view_explosion_desc = {
        .aspect = WGPUTextureAspect_All,
        .baseArrayLayer = 0,
        .arrayLayerCount = 1,
        .baseMipLevel = 0,
        .mipLevelCount = 1,
        .dimension = WGPUTextureViewDimension_2D,
        .format = texture_explosion_desc.format
    };
    WGPUTextureView texture_view_explosion = wgpuTextureCreateView(s->texture_explosion, &texture_view_explosion_desc);

    WGPUSamplerDescriptor sampler_desc = {
        .addressModeU = WGPUAddressMode_ClampToEdge,
        .addressModeV = WGPUAddressMode_Repeat,
        .addressModeW = WGPUAddressMode_ClampToEdge,
        .magFilter = WGPUFilterMode_Linear,
        .minFilter = WGPUFilterMode_Linear,
        .mipmapFilter = WGPUMipmapFilterMode_Linear,
        .lodMinClamp = 0.0f,
        .lodMaxClamp = 1.0f,
        .compare = WGPUCompareFunction_Undefined,
        .maxAnisotropy = 16
    };

    WGPUSampler sampler = wgpuDeviceCreateSampler(s->device, &sampler_desc);

    // ===================
    // === BIND GROUPS ===
    // ===================

    WGPUBindGroupEntry bg_asphalt_entries[BG_ENTRY_COUNT] = {
        {
            .binding = 0,
            .buffer = s->uniform_buffer,
            .offset = 0,
            .size = sizeof(Uniforms)
        },
        {
            .binding = 1,
            .sampler = sampler
        },
        {
            .binding = 2,
            .textureView = texture_view_asphalt
        },
    };

    WGPUBindGroupDescriptor bg_asphalt_desc = {
        .nextInChain = NULL,
        .layout = bgl,
        .entryCount = BG_ENTRY_COUNT,
        .entries = bg_asphalt_entries
    };
    s->bg_asphalt = wgpuDeviceCreateBindGroup(s->device, &bg_asphalt_desc);


    WGPUBindGroupEntry bg_explosion_entries[BG_ENTRY_COUNT] = {
        {
            .binding = 0,
            .buffer = s->uniform_buffer,
            .offset = 0,
            .size = sizeof(Uniforms)
        },
        {
            .binding = 1,
            .sampler = sampler
        },
        {
            .binding = 2,
            .textureView = texture_view_explosion
        },
    };

    WGPUBindGroupDescriptor bg_explosion_desc = {
        .nextInChain = NULL,
        .layout = bgl,
        .entryCount = BG_ENTRY_COUNT,
        .entries = bg_explosion_entries
    };
    s->bg_explosion = wgpuDeviceCreateBindGroup(s->device, &bg_explosion_desc);

    wgpuBindGroupLayoutRelease(bgl);

    // ================
    // === PIPELINE ===
    // ================

#define VERTEX_ATTRIBUTE_COUNT 2

    WGPUVertexAttribute vertex_attributes[VERTEX_ATTRIBUTE_COUNT] = {
        {
            .format = WGPUVertexFormat_Float32x3,
            .offset = 0,
            .shaderLocation = 0,
        },
        {
            .format = WGPUVertexFormat_Float32x2,
            .offset = 3 * sizeof(float),
            .shaderLocation = 1
        }
    };

    WGPUVertexBufferLayout vertex_buffer_layout = {
        .stepMode = WGPUVertexStepMode_Vertex,
        .arrayStride = VERTEX_BUFFER_STRIDE * sizeof(float),
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
        .vertex.buffers = &vertex_buffer_layout,
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

void render(State *s) {
    WGPUSurfaceTexture surface_texture;
    wgpuSurfaceGetCurrentTexture(s->surface, &surface_texture);
    if (surface_texture.status != WGPUSurfaceGetCurrentTextureStatus_SuccessOptimal) {
        return;
    }

    WGPUCommandEncoderDescriptor encoder_desc = {
        .nextInChain = NULL,
    };
    WGPUCommandEncoder encoder = wgpuDeviceCreateCommandEncoder(s->device, &encoder_desc);

    WGPUTextureViewDescriptor view_desc = {
        .nextInChain = NULL,
        .format = wgpuTextureGetFormat(surface_texture.texture),
        .dimension = WGPUTextureViewDimension_2D,
        .baseMipLevel = 0,
        .mipLevelCount = 1,
        .baseArrayLayer = 0,
        .arrayLayerCount = 1,
        .aspect = WGPUTextureAspect_All,
        .usage = WGPUTextureUsage_RenderAttachment
    };
    WGPUTextureView texture_view = texture_view = wgpuTextureCreateView(surface_texture.texture, &view_desc);

    WGPURenderPassColorAttachment render_pass_color_attachment = {
        .view = texture_view,
        .resolveTarget = NULL,
        .loadOp = WGPULoadOp_Clear,
        .storeOp = WGPUStoreOp_Store,
        .depthSlice = WGPU_DEPTH_SLICE_UNDEFINED,
        .clearValue = WGPUColor{ 0.5, 0.5, 0.5, 1.0 }
    };

    WGPURenderPassDescriptor render_pass_desc = {
        .nextInChain = NULL,
        .colorAttachmentCount = 1,
        .colorAttachments = &render_pass_color_attachment
    };

    // begin render pass
    WGPURenderPassEncoder render_pass = wgpuCommandEncoderBeginRenderPass(encoder, &render_pass_desc);

    wgpuRenderPassEncoderSetPipeline(render_pass, s->pipeline);

    wgpuRenderPassEncoderSetVertexBuffer(render_pass, 0, s->vertex_buffer, 0, WGPU_WHOLE_SIZE);
    wgpuRenderPassEncoderSetIndexBuffer(render_pass, s->index_buffer, WGPUIndexFormat_Uint32, 0, INDEX_COUNT_TOTAL * sizeof(int));
    wgpuRenderPassEncoderSetBindGroup(render_pass, 0, s->bg_asphalt, 0, NULL);
    wgpuRenderPassEncoderDrawIndexed(render_pass, INDEX_COUNT_QUAD, 1, 0, 0, 0);

    wgpuRenderPassEncoderSetBindGroup(render_pass, 0, s->bg_explosion, 0, NULL);
    wgpuRenderPassEncoderDrawIndexed(render_pass, INDEX_COUNT_QUAD, 1, INDEX_COUNT_QUAD, 0, 0);

    ImGui_ImplWGPU_RenderDrawData(ImGui::GetDrawData(), render_pass);

    // end render pass
    wgpuRenderPassEncoderEnd(render_pass);
    wgpuRenderPassEncoderRelease(render_pass);

    WGPUCommandBufferDescriptor command_buffer_desc = {
        .nextInChain = NULL
    };
    WGPUCommandBuffer command_buffer = wgpuCommandEncoderFinish(encoder, &command_buffer_desc);
    wgpuCommandEncoderRelease(encoder);

    wgpuQueueSubmit(s->queue, 1, &command_buffer);
    wgpuCommandBufferRelease(command_buffer);

    wgpuSurfacePresent(s->surface);

    wgpuTextureViewRelease(texture_view);
}

void terminate(State *s)
{
    ImGui_ImplWGPU_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();

    SDL_Metal_DestroyView(s->metal_view);
    SDL_DestroyWindow(s->window);
    SDL_Quit();

    wgpuSurfaceRelease(s->surface);
    wgpuBufferRelease(s->vertex_buffer);
    wgpuBufferRelease(s->uniform_buffer);
    wgpuBindGroupRelease(s->bg_asphalt);
    wgpuBindGroupRelease(s->bg_explosion);
    wgpuAdapterRelease(s->adapter);
    wgpuDeviceRelease(s->device);
    wgpuInstanceRelease(s->instance);
    wgpuRenderPipelineRelease(s->pipeline);
}

int main() {
    State s = {
        .window = NULL,
        .metal_view = NULL,
        .adapter = NULL,
        .device = NULL,
        .instance = NULL,
        .surface = NULL,
        .pipeline = NULL,
        .queue = NULL,
        .uniform_buffer = NULL,
        .vertex_buffer = NULL,
        .index_buffer = NULL,
        .vertex_buffer_size = 0,
        .texture_asphalt = NULL,
        .bg_asphalt = NULL,
        .bg_explosion = NULL
    };

    Imgui imgui_state = {
        .anisotropy = 1,
        .camera_pan = 0.0f
    };

    initialize(&s);

    const float vertices[VERTEX_COUNT_TOTAL * VERTEX_BUFFER_STRIDE] = {
        // asphalt
		-10.0f, 0.0f, -10.0f, 0.0f, 0.0f,
		-10.0f, 0.0f, -330.0f, 0.0f, 15.0f,
		10.0f,  0.0f, -330.0f, 1.0f, 15.0f,
		10.0f,  0.0f, -10.0f, 1.0f, 0.0f,
        // explosion
        2.0f, 0.0f, -22.0f, 0.0f, 0.0f,
        2.0f, 10.0f, -22.0f, 0.0f, 1.0f,
        12.0f, 10.0f, -22.0f, 1.0f, 1.0f,
        12.0f, 0.0f, -22.0f, 1.0f, 0.0f
    };
    s.vertex_buffer_size = sizeof(vertices);
    wgpuQueueWriteBuffer(s.queue, s.vertex_buffer, 0, vertices, sizeof(vertices));

    const int indices[INDEX_COUNT_TOTAL] = {
        // asphalt
        0, 1, 2,
        0, 2, 3,
        // explosion
        4, 5, 6,
        4, 6, 7
    };
    wgpuQueueWriteBuffer(s.queue, s.index_buffer, 0, indices, sizeof(indices));

    Uniforms uniform_buffer_state = {
        .time = 0,
    };

    float fovy = GLM_PI/4;
    float aspect_ratio = (float)WINDOW_WIDTH / (float)WINDOW_HEIGHT;
    float near_plane = 0.01f;
    float far_plane = 400.0f;
    glm_perspective(fovy, aspect_ratio, near_plane, far_plane, uniform_buffer_state.projection_matrix);

    uint64_t freq = SDL_GetPerformanceFrequency();
    bool running = true;
    while (running) {
        vec3 camera_position = {imgui_state.camera_pan, 10, 0};
        glm_vec3_copy(camera_position, uniform_buffer_state.camera_position);

        wgpuQueueWriteBuffer(s.queue, s.uniform_buffer, 0, &uniform_buffer_state, sizeof(Uniforms));

        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_EVENT_QUIT) running = false;
            ImGui_ImplSDL3_ProcessEvent(&e);
        }

        float time = (float)(SDL_GetPerformanceCounter() / (float)freq);
        uniform_buffer_state.time = time;
        wgpuQueueWriteBuffer(s.queue, s.uniform_buffer, 0, &uniform_buffer_state, sizeof(Uniforms));

        render_imgui(&imgui_state);

        render(&s);
    }

    terminate(&s);
}
