#include <stdio.h>
#include <webgpu.h>
#include <SDL3/SDL.h>
#include <utility>

#include <iostream>
#include <cassert>
#include <vector>

#define PATH_VERTEX_SHADER "build/vert.spv"
#define PATH_FRAGMENT_SHADER "build/frag.spv"

#define WINDOW_WIDTH 800
#define WINDOW_HEIGHT 600
#define WEBGPU_STR(str) (WGPUStringView){.data = str, .length = sizeof(str) - 1}



typedef struct DeviceRequestState
{
    WGPUDevice *device = NULL;
    bool request_ended = false;
} DeviceRequestState;

typedef struct AdapterRequestState
{
    WGPUAdapter adapter = NULL;
    bool request_ended = false;
} AdapterRequestState;


void initialize_window(SDL_Window **window, void **metal_layer)
{
    // create window
    SDL_Init(SDL_INIT_VIDEO);
    *window = SDL_CreateWindow("a", WINDOW_WIDTH, WINDOW_HEIGHT, SDL_WINDOW_METAL);
    SDL_MetalView metal_view = SDL_Metal_CreateView(*window);
    *metal_layer = SDL_Metal_GetLayer(metal_view);
}

void initialize_instance(WGPUInstance *instance)
{
    // create instance
    WGPUInstanceDescriptor instance_desc = {0};
    instance_desc.nextInChain = NULL;
    *instance = wgpuCreateInstance(&instance_desc);
}


void on_adapter_request_ended(
    WGPURequestAdapterStatus status,
    WGPUAdapter adapter,
    WGPUStringView message,
    void *userdata1,
    void *userdata2)
{
    AdapterRequestState *state = (AdapterRequestState *)userdata1;
    state->adapter = adapter;
    state->request_ended = true;

    if (status == WGPURequestAdapterStatus_Success)
    {
        printf("Adapter request succeeded\n");
        // print_adapter_info(adapter);
    }
    else
    {
        fprintf(stderr,
                "Adapter request failed: %.*s\n",
                (int)message.length, message.data);
    }
}

void on_device_request_ended(
    WGPURequestDeviceStatus status,
    WGPUDevice device,
    WGPUStringView message,
    void *userdata1,
    void *userdata2)
{
    DeviceRequestState *device_request_state = (DeviceRequestState *)userdata1;
    *(device_request_state->device) = device;
    device_request_state->request_ended = true;
    if (status == WGPURequestDeviceStatus_Success)
    {
        printf("Device request succeeded\n");
        // print_device_info(device);
    }
    else
    {
        fprintf(stderr,
                "Device request failed: %.*s\n",
                (int)message.length, message.data);
    }
}

void initialize_device(WGPUInstance *instance, WGPUDevice *device, WGPUSurface *surface, WGPUAdapter *adapter)
{
    // get adapter
    struct AdapterRequestState adapter_request_state;
    WGPURequestAdapterCallbackInfo adapter_callback_info = {0};
    adapter_callback_info.callback = on_adapter_request_ended;
    adapter_callback_info.userdata1 = &adapter_request_state;
    adapter_callback_info.mode = WGPUCallbackMode_AllowProcessEvents;
    WGPURequestAdapterOptions adapter_request_options = {0};
    adapter_request_options.nextInChain = NULL;
    adapter_request_options.featureLevel = WGPUFeatureLevel_Core;
    adapter_request_options.compatibleSurface = *surface;
    wgpuInstanceRequestAdapter(*instance, &adapter_request_options, adapter_callback_info);
    while (!adapter_request_state.request_ended)
    {
        wgpuInstanceProcessEvents(*instance);
    }

    // get device
    DeviceRequestState device_request_state;
    device_request_state.device = device;
    WGPUDeviceDescriptor device_desc = {0};
    device_desc.nextInChain = NULL;
    device_desc.defaultQueue.nextInChain = NULL;
    device_desc.deviceLostCallbackInfo = {0}; // TODO: device lost callback
    WGPURequestDeviceCallbackInfo device_callback_info = {0};
    device_callback_info.nextInChain = NULL;
    device_callback_info.callback = on_device_request_ended;
    device_callback_info.userdata1 = &device_request_state;
    device_callback_info.mode = WGPUCallbackMode_AllowProcessEvents;
    wgpuAdapterRequestDevice(adapter_request_state.adapter, &device_desc, device_callback_info);
    while (!device_request_state.request_ended)
    { // TODO: prettier busywait
        wgpuInstanceProcessEvents(*instance);
    }

    *adapter = adapter_request_state.adapter;
}

void initialize_surface(WGPUInstance *instance, void *metal_layer, WGPUSurface *surface)
{
    // create surface
    WGPUSurfaceSourceMetalLayer src = {
        .chain = {.next = NULL, .sType = WGPUSType_SurfaceSourceMetalLayer},
        .layer = metal_layer};
    WGPUSurfaceDescriptor surface_desc = {0};
    surface_desc.nextInChain = (const WGPUChainedStruct *)&src;
    *surface = wgpuInstanceCreateSurface(*instance, &surface_desc);
}


void load_spirv(const char* path, uint32_t** out_data, size_t* out_word_count) {
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


void initialize_pipeline(WGPURenderPipeline *pipeline, WGPUDevice *device,WGPUPipelineLayout * pipeline_layout ,WGPUSurfaceCapabilities surface_capabilities)
{
    
    //Initialize vertex shading module
    size_t vertex_shader_words = 0;
    uint32_t *vertex_shader_source = NULL;
    load_spirv(PATH_VERTEX_SHADER, &vertex_shader_source, &vertex_shader_words);
    WGPUShaderSourceSPIRV vertex_shader_spirv = {};
    vertex_shader_spirv.chain.next = NULL;
    vertex_shader_spirv.chain.sType = WGPUSType_ShaderSourceSPIRV;
    vertex_shader_spirv.codeSize = vertex_shader_words;
    vertex_shader_spirv.code = vertex_shader_source;
    WGPUShaderModuleDescriptor vertex_shader_desc = {};
    vertex_shader_desc.nextInChain = &vertex_shader_spirv.chain;
    WGPUShaderModule vertex_shader_module = wgpuDeviceCreateShaderModule(*device, &vertex_shader_desc);
    free(vertex_shader_source);


    //Initialize fragment shading module
    size_t fragment_shader_words = 0;
    uint32_t *fragment_shader_source = NULL;
    load_spirv(PATH_FRAGMENT_SHADER, &fragment_shader_source, &fragment_shader_words);
    WGPUShaderSourceSPIRV fragment_shader_spirv = {};
    fragment_shader_spirv.chain.next = NULL;
    fragment_shader_spirv.chain.sType = WGPUSType_ShaderSourceSPIRV;
    fragment_shader_spirv.codeSize = fragment_shader_words;
    fragment_shader_spirv.code = fragment_shader_source;
    WGPUShaderModuleDescriptor fragment_shader_desc = {};
    fragment_shader_desc.nextInChain = &fragment_shader_spirv.chain;
    WGPUShaderModule fragment_shader_module = wgpuDeviceCreateShaderModule(*device, &fragment_shader_desc);
    free(fragment_shader_source);


    WGPUShaderModuleDescriptor shaderDesc{};
    WGPURenderPipelineDescriptor pipelineDesc{};
    pipelineDesc.nextInChain = NULL;

    // vertex
    
    // WGPUVertexBufferLayout * vertex_buffer_layouts = get_vertex_buffer_layouts();
    WGPUVertexAttribute vertex_attributes[2];
    vertex_attributes[0].format = WGPUVertexFormat_Float32x3;
    vertex_attributes[0].offset = 0;
    vertex_attributes[0].shaderLocation = 0;
    vertex_attributes[1].format = WGPUVertexFormat_Float32x3;
    vertex_attributes[1].offset = 3 * sizeof(float);
    vertex_attributes[1].shaderLocation = 1;
    
    WGPUVertexBufferLayout vertex_buffer_layout = {};
    vertex_buffer_layout.stepMode = WGPUVertexStepMode_Vertex;
    vertex_buffer_layout.arrayStride = 6 * sizeof(float);
    vertex_buffer_layout.attributeCount = 2;
    vertex_buffer_layout.attributes = vertex_attributes;

    pipelineDesc.vertex.module = vertex_shader_module;
    pipelineDesc.vertex.entryPoint = WEBGPU_STR("main");
    pipelineDesc.vertex.constantCount = 0;
    pipelineDesc.vertex.constants = NULL;
    pipelineDesc.vertex.bufferCount = 1;
    pipelineDesc.vertex.buffers = &vertex_buffer_layout;


    // [...] Describe primitive pipeline state
    pipelineDesc.primitive.topology = WGPUPrimitiveTopology_TriangleList;
    pipelineDesc.primitive.stripIndexFormat = WGPUIndexFormat_Undefined;
    pipelineDesc.primitive.frontFace = WGPUFrontFace_CCW;
    pipelineDesc.primitive.cullMode = WGPUCullMode_None;
    
    // [...] Describe stencil/depth pipeline state
    pipelineDesc.depthStencil = NULL;
    
    // [...] Describe fragment pipeline state
    WGPUFragmentState fragmentState{};
    fragmentState.module = fragment_shader_module;
    fragmentState.entryPoint = WEBGPU_STR("main");
    fragmentState.constantCount = 0;
    fragmentState.constants = nullptr;
    
    // [...] We'll configure the blending stage here
    pipelineDesc.fragment = &fragmentState;
    
    // [...] Describe multi-sampling state
    WGPUBlendState blendState{};
    blendState.color.srcFactor = WGPUBlendFactor_SrcAlpha;
    blendState.color.dstFactor = WGPUBlendFactor_OneMinusSrcAlpha;
    blendState.color.operation = WGPUBlendOperation_Add;
    blendState.alpha.srcFactor = WGPUBlendFactor_Zero;
    blendState.alpha.dstFactor = WGPUBlendFactor_One;
    blendState.alpha.operation = WGPUBlendOperation_Add;


    
    WGPUTextureFormat surface_format = WGPUTextureFormat_Undefined;
    if (surface_capabilities.formatCount > 0)
    {
        surface_format = surface_capabilities.formats[0];
    }

    WGPUColorTargetState colorTarget{};
    colorTarget.format = surface_format;
    colorTarget.blend = &blendState;
    colorTarget.writeMask = WGPUColorWriteMask_All; // We could write to only some of the color channels.

    fragmentState.targetCount = 1;
    fragmentState.targets = &colorTarget;


    pipelineDesc.multisample.count = 1;
    // Default value for the mask, meaning "all bits on"
    pipelineDesc.multisample.mask = ~0u;
    // Default value as well (irrelevant for count = 1 anyways)
    pipelineDesc.multisample.alphaToCoverageEnabled = false;

    pipelineDesc.layout = * pipeline_layout;
    *pipeline = wgpuDeviceCreateRenderPipeline(*device, &pipelineDesc);

    wgpuShaderModuleRelease(vertex_shader_module);
    wgpuShaderModuleRelease(fragment_shader_module);
}

void initialize_layout(WGPUDevice *device, WGPUPipelineLayout * pipeline_layout, WGPUBindGroupLayout * bind_group_layout) {
    // Define binding layout
    WGPUBindGroupLayoutEntry bindingLayout = {};

    // The binding index as used in the @binding attribute in the shader
    bindingLayout.binding = 0;

    // The stage that needs to access this resource
    bindingLayout.visibility = WGPUShaderStage_Fragment;
    bindingLayout.buffer.type = WGPUBufferBindingType_Uniform;


    // Create a bind group layout
    WGPUBindGroupLayoutDescriptor bindGroupLayoutDesc = {};
    bindGroupLayoutDesc.nextInChain = NULL;
    bindGroupLayoutDesc.entryCount = 1;
    bindGroupLayoutDesc.entries = &bindingLayout;
    * bind_group_layout = wgpuDeviceCreateBindGroupLayout(*device, &bindGroupLayoutDesc);

    // Create the pipeline layout
    WGPUPipelineLayoutDescriptor layoutDesc = {};
    layoutDesc.nextInChain = NULL;
    layoutDesc.bindGroupLayoutCount = 1;
    layoutDesc.bindGroupLayouts = bind_group_layout;
    *pipeline_layout = wgpuDeviceCreatePipelineLayout(*device, &layoutDesc);

}
