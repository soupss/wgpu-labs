#include "init.hpp"
#include <stdlib.h>
#include <stdio.h>
#include <webgpu.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_wgpu.h>
#include "constants.h"
#include "util.h"
#include "model.h"
#include "bind_group.h"
#include "texture_manager.h"

typedef struct {
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

typedef struct {
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
    SDL_SetWindowRelativeMouseMode(s->window, true);

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
    s->shadermodule_comp = wgpuDeviceCreateShaderModule(s->device, &compute_shader_desc);
    free(compute_shader_source);

    // ===============
    // === LAYOUTS ===
    // ===============

    WGPUBindGroupLayout bgls[BG_COUNT];
    bgls_create(s, bgls);

    // ================
    // === TEXTURES ===
    // ================

    s->tm = (TextureManager*)malloc(sizeof(TextureManager));
    texture_manager_init(s, s->tm);

    model_load(s, bgls[1], &s->model_car, PATH_MODEL_CAR, NULL);
    model_load(s, bgls[1], &s->model_city, PATH_MODEL_CITY, DIR_CITY_TEXTURES);
    model_load(s, bgls[1], &s->model_ground, PATH_MODEL_GROUND, NULL);

    // ===================
    // === BIND GROUPS ===
    // ===================

    bg_create_frame(s, bgls[0]);

    // ================
    // === PIPELINE ===
    // ================

    WGPUPipelineLayoutDescriptor pipeline_layout_desc = {
        .nextInChain = NULL,
        .bindGroupLayoutCount = 2,
        .bindGroupLayouts = bgls
    };
    WGPUPipelineLayout pipeline_layout = wgpuDeviceCreatePipelineLayout(s->device, &pipeline_layout_desc);


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

    WGPUVertexBufferLayout vbo_layout = {
        .stepMode = WGPUVertexStepMode_Vertex,
        .arrayStride = (3 + 3 + 2) * sizeof(float),
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

    WGPUDepthStencilState ds = {
        .format = WGPUTextureFormat_Depth24Plus,
        .depthWriteEnabled = WGPUOptionalBool_True,
        .depthCompare = WGPUCompareFunction_Less,
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
        .vertex.buffers = &vbo_layout,
        .vertex.module = vertex_shader_module,

        .primitive.topology = WGPUPrimitiveTopology_TriangleList,
        .primitive.stripIndexFormat = WGPUIndexFormat_Undefined,
        .primitive.frontFace = WGPUFrontFace_CCW,
        .primitive.cullMode = WGPUCullMode_Back,

        .fragment = &fragment_state,

        .layout = pipeline_layout,
        .depthStencil = &ds,
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
