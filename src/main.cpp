#include "imgui_impl_sdl3.h"
#include "imgui_impl_wgpu.h"
#include "util.hpp"
#include <stdlib.h>
#include <stdio.h>
#include <webgpu.h>
#include <SDL3/SDL.h>

#define PATH_VERTEX_SHADER "build/vert.spv"
#define PATH_FRAGMENT_SHADER "build/frag.spv"

#define WINDOW_WIDTH 1200
#define WINDOW_HEIGHT 800
#define VERTICES_MAX 100
#define VERTEX_BUFFER_STRIDE 6

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
    size_t vertex_buffer_size;
    WGPUBindGroup bind_group;
} State;

typedef struct Uniforms {
    float time;
    float _pad[3] = {0};
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

    WGPUSurfaceSourceMetalLayer src = {
        .chain = { .next = NULL, .sType = WGPUSType_SurfaceSourceMetalLayer },
        .layer = metal_layer
    };

    WGPUSurfaceDescriptor surface_desc = {
        .nextInChain = (const WGPUChainedStruct*)&src
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
    load_spirv(PATH_VERTEX_SHADER, &vertex_shader_source, &vertex_shader_words);
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
    load_spirv(PATH_FRAGMENT_SHADER, &fragment_shader_source, &fragment_shader_words);
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

    // ===============
    // === LAYOUTS ===
    // ===============

    WGPUBindGroupLayoutEntry bind_group_layout_entry = {
        .binding = 0,
        .visibility = WGPUShaderStage_Fragment | WGPUShaderStage_Vertex,
        .buffer.type = WGPUBufferBindingType_Uniform
    };

    WGPUBindGroupLayoutDescriptor bind_group_layout_desc = {
        .nextInChain = NULL,
        .entryCount = 1,
        .entries = &bind_group_layout_entry
    };
    WGPUBindGroupLayout bind_group_layout = wgpuDeviceCreateBindGroupLayout(s->device, &bind_group_layout_desc);

    WGPUPipelineLayoutDescriptor pipeline_layout_desc = {
        .nextInChain = NULL,
        .bindGroupLayoutCount = 1,
        .bindGroupLayouts = &bind_group_layout
    };
    WGPUPipelineLayout pipeline_layout = wgpuDeviceCreatePipelineLayout(s->device, &pipeline_layout_desc);

    // ===============
    // === BUFFERS ===
    // ===============

    WGPUBufferDescriptor vertex_buffer_desc = {
        .nextInChain = NULL,
        .usage = WGPUBufferUsage_Vertex | WGPUBufferUsage_CopyDst,
        .size = VERTICES_MAX * VERTEX_BUFFER_STRIDE,
        .mappedAtCreation = false
    };
    s->vertex_buffer = wgpuDeviceCreateBuffer(s->device, &vertex_buffer_desc);

    WGPUBufferDescriptor uniform_buffer_desc = {
        .nextInChain = NULL,
        .usage = WGPUBufferUsage_Uniform | WGPUBufferUsage_CopyDst,
        .size = sizeof(Uniforms),
        .mappedAtCreation = false
    };
    s->uniform_buffer = wgpuDeviceCreateBuffer(s->device, &uniform_buffer_desc);

    // ===================
    // === BIND GROUPS ===
    // ===================

    WGPUBindGroupEntry bind_group_entry = {
        .binding = 0,
        .buffer = s->uniform_buffer,
        .offset = 0,
        .size = sizeof(Uniforms)
    };

    WGPUBindGroupDescriptor bind_group_desc = {
        .nextInChain = NULL,
        .layout = bind_group_layout,
        .entryCount = 1,
        .entries = &bind_group_entry
    };
    s->bind_group = wgpuDeviceCreateBindGroup(s->device, &bind_group_desc);

    wgpuBindGroupLayoutRelease(bind_group_layout);

    // ================
    // === PIPELINE ===
    // ================

    WGPUVertexAttribute vertex_attributes[2] = {
        {
            .format = WGPUVertexFormat_Float32x3,
            .offset = 0,
            .shaderLocation = 0,
        },
        {
            .format = WGPUVertexFormat_Float32x3,
            .offset = 3 * sizeof(float),
            .shaderLocation = 1
        }
    };

    WGPUVertexBufferLayout vertex_buffer_layout = {
        .stepMode = WGPUVertexStepMode_Vertex,
        .arrayStride = VERTEX_BUFFER_STRIDE * sizeof(float),
        .attributeCount = 2,
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
        .primitive.cullMode = WGPUCullMode_None,

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
        wgpuRenderPassEncoderSetBindGroup(render_pass, 0, s->bind_group, 0, NULL);
        wgpuRenderPassEncoderDraw(render_pass, 6, 1, 0, 0);

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
    wgpuBindGroupRelease(s->bind_group);
    wgpuAdapterRelease(s->adapter);
    wgpuDeviceRelease(s->device);
    wgpuInstanceRelease(s->instance);
    wgpuRenderPipelineRelease(s->pipeline);
}

void imgui() {
    ImGui_ImplSDL3_NewFrame();
    ImGui_ImplWGPU_NewFrame();
    ImGui::NewFrame();

    ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);

    ImGui::Render();
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
        .vertex_buffer_size = 0,
        .bind_group = NULL
    };

    initialize(&s);

    const float vertices[] = {
        -0.8f,  0.1f,   1.0f, 0.0f, 1.0f, 0.0f,
        0.8f,  0.1f,   1.0f, 0.0f, 0.0f, 0.2f,
        0.1f, 0.8f,   1.0f, 0.0f, 0.0f, 0.2f,
        -0.7f,  -0.1,  1.0f, 1.0f, 0.0f, 0.0f,
        0.9f,  -0.1f,   1.0f, 1.0f, 1.0f, 0.0f,
        -0.1f,  -0.8, 1.0f, 0.0f, 1.0f, 1.0f
    };
    s.vertex_buffer_size = sizeof(vertices);
    wgpuQueueWriteBuffer(s.queue, s.vertex_buffer, 0, vertices, sizeof(vertices));

    Uniforms uniform_buffer_state = {
        .time = 0
    };
    wgpuQueueWriteBuffer(s.queue, s.uniform_buffer, 0, &uniform_buffer_state, sizeof(Uniforms));

    uint64_t freq = SDL_GetPerformanceFrequency();
    bool running = true;
    while (running) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_EVENT_QUIT) running = false;
            ImGui_ImplSDL3_ProcessEvent(&e);
        }

        float time = (float)(SDL_GetPerformanceCounter() / (float)freq);
        uniform_buffer_state.time = time;
        wgpuQueueWriteBuffer(s.queue, s.uniform_buffer, 0, &uniform_buffer_state, sizeof(Uniforms));

        imgui();

        render(&s);
    }

    terminate(&s);
}
