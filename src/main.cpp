#include "imgui_impl_sdl3.h"
#include "imgui_impl_wgpu.h"
#include <stdlib.h>
#include <stdio.h>
#include <webgpu.h>
#include <SDL3/SDL.h>

#define PATH_VERTEX_SHADER "build/vert.spv"
#define PATH_FRAGMENT_SHADER "build/frag.spv"

#define WINDOW_WIDTH 1200
#define WINDOW_HEIGHT 800

float clear_color[3] = { 0.2f, 0.2f, 0.8f };
float triangle_color[4] = { 1.0f, 1.0f, 1.0f, 1.0f};

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

typedef struct AdapterRequest {
    WGPUAdapter *adapter = NULL;
    bool request_ended = false;
} AdapterRequest;

void print_adapter_info(WGPUAdapter adapter) {
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
    WGPUDevice *device = NULL;
    bool request_ended = false;
} DeviceRequest;

void print_device_info(WGPUDevice device) {
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

bool get_next_texture_view(WGPUSurface const surface, WGPUSurfaceTexture *surface_texture, WGPUTextureView *texture_view) {
    *texture_view = NULL;
    wgpuSurfaceGetCurrentTexture(surface, surface_texture);
    if (surface_texture->status != WGPUSurfaceGetCurrentTextureStatus_SuccessOptimal) {
        return false;
    }
    WGPUTextureViewDescriptor view_desc = {};
    view_desc.nextInChain = NULL;
    view_desc.format = wgpuTextureGetFormat(surface_texture->texture);
    view_desc.dimension = WGPUTextureViewDimension_2D;
    view_desc.baseMipLevel = 0;
    view_desc.mipLevelCount = 1;
    view_desc.baseArrayLayer = 0;
    view_desc.arrayLayerCount = 1;
    view_desc.aspect = WGPUTextureAspect_All;
    view_desc.usage = WGPUTextureUsage_RenderAttachment;
    *texture_view = wgpuTextureCreateView(surface_texture->texture, &view_desc);
    return true;
}

void initialize(
    SDL_Window **window,
    SDL_MetalView *metal_view,
    WGPUInstance *instance,
    WGPUAdapter *adapter,
    WGPUDevice *device,
    WGPUSurface *surface,
    WGPURenderPipeline *pipeline,
    WGPUQueue *queue,
    WGPUBuffer *uniform_buffer,
    WGPUBindGroup *bind_group,
    WGPUBuffer *uniform_buffer2,
    WGPUBindGroup *bind_group2)
{
    // create window
    SDL_Init(SDL_INIT_VIDEO);
    *window = SDL_CreateWindow("a", WINDOW_WIDTH, WINDOW_HEIGHT, SDL_WINDOW_METAL);
    *metal_view = SDL_Metal_CreateView(*window);
    void* metal_layer = SDL_Metal_GetLayer(*metal_view);

    // create instance
    WGPUInstanceDescriptor instance_desc = {};
    instance_desc.nextInChain= NULL;
    *instance = wgpuCreateInstance(&instance_desc);

    // create surface
    WGPUSurfaceSourceMetalLayer src = {
        .chain = { .next = NULL, .sType = WGPUSType_SurfaceSourceMetalLayer },
        .layer = metal_layer
    };
    WGPUSurfaceDescriptor surface_desc = {};
    surface_desc.nextInChain = (const WGPUChainedStruct*)&src;
    *surface = wgpuInstanceCreateSurface(*instance, &surface_desc);

    // get adapter
    AdapterRequest adapter_request_state = {};
    adapter_request_state.adapter = adapter;
    WGPURequestAdapterCallbackInfo adapter_callback_info = {};
    adapter_callback_info.callback = on_adapter_request_ended;
    adapter_callback_info.userdata1 = &adapter_request_state;
    adapter_callback_info.mode = WGPUCallbackMode_AllowProcessEvents;
    WGPURequestAdapterOptions adapter_request_options = {};
    adapter_request_options.nextInChain = NULL;
    adapter_request_options.featureLevel = WGPUFeatureLevel_Core;
    adapter_request_options.compatibleSurface = *surface;
    wgpuInstanceRequestAdapter(*instance, &adapter_request_options, adapter_callback_info);
    while(!adapter_request_state.request_ended) {
        wgpuInstanceProcessEvents(*instance);
    }

    // get device
    DeviceRequest device_request_state = {};
    device_request_state.device = device;
    WGPUDeviceDescriptor device_desc = {};
    device_desc.nextInChain = NULL;
    device_desc.defaultQueue.nextInChain = NULL;
    device_desc.deviceLostCallbackInfo = {}; // TODO: device lost callback
    WGPURequestDeviceCallbackInfo device_callback_info = {};
    device_callback_info.nextInChain = NULL;
    device_callback_info.callback = on_device_request_ended;
    device_callback_info.userdata1 = &device_request_state;
    device_callback_info.mode  = WGPUCallbackMode_AllowProcessEvents;
    wgpuAdapterRequestDevice(*adapter, &device_desc, device_callback_info);
    while (!device_request_state.request_ended) { // TODO: prettier busywait
        wgpuInstanceProcessEvents(*instance);
    }

    // configure surface
    WGPUSurfaceConfiguration surface_config = {};
    surface_config.nextInChain = NULL;
    surface_config.width = WINDOW_WIDTH;
    surface_config.height = WINDOW_HEIGHT;

    WGPUSurfaceCapabilities surface_capabilities = {};
    wgpuSurfaceGetCapabilities(*surface, *adapter, &surface_capabilities);
    WGPUTextureFormat surface_format = WGPUTextureFormat_Undefined;
    if (surface_capabilities.formatCount > 0) {
        surface_format = surface_capabilities.formats[0];
    }
    wgpuSurfaceCapabilitiesFreeMembers(surface_capabilities);
    surface_config.format = surface_format;
    surface_config.viewFormatCount = 0;
    surface_config.viewFormats = NULL;
    surface_config.usage = WGPUTextureUsage_RenderAttachment;
    surface_config.device = *device;
    surface_config.presentMode = WGPUPresentMode_Fifo;
    surface_config.alphaMode = WGPUCompositeAlphaMode_Auto;
    wgpuSurfaceConfigure(*surface, &surface_config);

    *queue = wgpuDeviceGetQueue(*device);


    WGPUBufferDescriptor uniform_buffer_desc = {};
    uniform_buffer_desc.nextInChain = NULL;
    uniform_buffer_desc.usage = WGPUBufferUsage_Uniform | WGPUBufferUsage_CopyDst;
    uniform_buffer_desc.size = sizeof(triangle_color);
    uniform_buffer_desc.mappedAtCreation = false;
    *uniform_buffer = wgpuDeviceCreateBuffer(*device, &uniform_buffer_desc);

    WGPUBufferDescriptor uniform_buffer2_desc = {};
    uniform_buffer2_desc.nextInChain = NULL;
    uniform_buffer2_desc.usage = WGPUBufferUsage_Uniform | WGPUBufferUsage_CopyDst;
    uniform_buffer2_desc.size = sizeof(triangle_color);
    uniform_buffer2_desc.mappedAtCreation = false;
    *uniform_buffer2 = wgpuDeviceCreateBuffer(*device, &uniform_buffer2_desc);

    WGPUBindGroupLayoutEntry bind_group_layout_entry = {};
    bind_group_layout_entry.binding = 0;
    bind_group_layout_entry.visibility = WGPUShaderStage_Fragment;
    bind_group_layout_entry.buffer.type = WGPUBufferBindingType_Uniform;

    WGPUBindGroupLayoutDescriptor bind_group_layout_desc = {};
    bind_group_layout_desc.nextInChain = NULL;
    bind_group_layout_desc.entryCount = 1;
    bind_group_layout_desc.entries = &bind_group_layout_entry;
    WGPUBindGroupLayout bind_group_layout = wgpuDeviceCreateBindGroupLayout(*device, &bind_group_layout_desc);

    WGPUPipelineLayoutDescriptor pipeline_layout_desc = {};
    pipeline_layout_desc.nextInChain = NULL;
    pipeline_layout_desc.bindGroupLayoutCount = 1;
    pipeline_layout_desc.bindGroupLayouts = &bind_group_layout;
    WGPUPipelineLayout pipeline_layout = wgpuDeviceCreatePipelineLayout(*device, &pipeline_layout_desc);

    WGPUBindGroupEntry bind_group_entry = {};
    bind_group_entry.binding = 0;
    bind_group_entry.buffer = *uniform_buffer;
    bind_group_entry.offset = 0;
    bind_group_entry.size = sizeof(triangle_color);

    WGPUBindGroupDescriptor bind_group_desc = {};
    bind_group_desc.nextInChain = NULL;
    bind_group_desc.layout = bind_group_layout;
    bind_group_desc.entryCount = 1;
    bind_group_desc.entries = &bind_group_entry;
    *bind_group = wgpuDeviceCreateBindGroup(*device, &bind_group_desc);

    WGPUBindGroupEntry bind_group2_entry = {};
    bind_group2_entry.binding = 0;
    bind_group2_entry.buffer = *uniform_buffer2;
    bind_group2_entry.offset = 0;
    bind_group2_entry.size = sizeof(triangle_color);

    WGPUBindGroupDescriptor bind_group2_desc = {};
    bind_group2_desc.nextInChain = NULL;
    bind_group2_desc.layout = bind_group_layout;
    bind_group2_desc.entryCount = 1;
    bind_group2_desc.entries = &bind_group2_entry;
    *bind_group2 = wgpuDeviceCreateBindGroup(*device, &bind_group2_desc);

    size_t vertex_shader_words = 0;
    uint32_t *vertex_shader_source = NULL;;
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

    WGPURenderPipelineDescriptor pipeline_desc = {};
    pipeline_desc.nextInChain = NULL;
    // vertex
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

    WGPUStringView vertex_entry_point = {};
    vertex_entry_point.data = "main";
    vertex_entry_point.length = WGPU_STRLEN;
    pipeline_desc.vertex.entryPoint = vertex_entry_point;
    pipeline_desc.vertex.constantCount = 0;
    pipeline_desc.vertex.constants = NULL;
    pipeline_desc.vertex.bufferCount = 1;
    pipeline_desc.vertex.buffers = &vertex_buffer_layout;
    pipeline_desc.vertex.module = vertex_shader_module;

    // primitive
    pipeline_desc.primitive.topology = WGPUPrimitiveTopology_TriangleList;
    pipeline_desc.primitive.stripIndexFormat = WGPUIndexFormat_Undefined;
    pipeline_desc.primitive.frontFace = WGPUFrontFace_CCW;
    pipeline_desc.primitive.cullMode = WGPUCullMode_None;
    // fragment
    WGPUFragmentState fragment_state = {};
    fragment_state.module = fragment_shader_module;
    WGPUStringView fragment_entry_point = {};
    fragment_entry_point.data = "main";
    fragment_entry_point.length = WGPU_STRLEN;
    fragment_state.entryPoint = fragment_entry_point;
    fragment_state.constantCount = 0;
    fragment_state.constants = NULL;
    pipeline_desc.fragment = &fragment_state;
    // stencil
    pipeline_desc.depthStencil = NULL;
    // blending
    WGPUBlendState blend_state = {};
    blend_state.color.srcFactor = WGPUBlendFactor_SrcAlpha;
    blend_state.color.dstFactor = WGPUBlendFactor_OneMinusSrcAlpha;
    blend_state.color.operation = WGPUBlendOperation_Add;
    blend_state.alpha.srcFactor = WGPUBlendFactor_Zero;
    blend_state.alpha.dstFactor = WGPUBlendFactor_One;
    blend_state.alpha.operation = WGPUBlendOperation_Add;
    WGPUColorTargetState color_target = {};
    color_target.format = surface_format;
    color_target.blend = &blend_state;
    color_target.writeMask = WGPUColorWriteMask_All;
    fragment_state.targetCount = 1;
    fragment_state.targets = &color_target;
    pipeline_desc.multisample.count = 1;
    pipeline_desc.multisample.mask = ~0u;
    pipeline_desc.multisample.alphaToCoverageEnabled = false;
    pipeline_desc.layout = pipeline_layout;

    *pipeline = wgpuDeviceCreateRenderPipeline(*device, &pipeline_desc);

    ImGui::CreateContext();
    ImGui_ImplSDL3_InitForMetal(*window);
    ImGui_ImplWGPU_InitInfo imgui_init = {};
    imgui_init.Device = *device;
    imgui_init.NumFramesInFlight = 3;
    imgui_init.RenderTargetFormat = surface_format;
    imgui_init.DepthStencilFormat = WGPUTextureFormat_Undefined;
    ImGui_ImplWGPU_Init(&imgui_init);

    wgpuShaderModuleRelease(vertex_shader_module);
    wgpuShaderModuleRelease(fragment_shader_module);
}

void render(WGPUDevice device, WGPUSurface surface, WGPURenderPipeline pipeline, WGPUQueue queue, WGPUBuffer vertex_buffer, size_t vertex_buffer_size, WGPUBuffer vertex_buffer2, size_t vertex_buffer2_size, WGPUBindGroup bind_group, WGPUBindGroup bind_group2) {
        WGPUCommandEncoderDescriptor encoder_desc = {};
        encoder_desc.nextInChain = NULL;
        WGPUCommandEncoder encoder = wgpuDeviceCreateCommandEncoder(device, &encoder_desc);

        WGPUSurfaceTexture surface_texture;
        WGPUTextureView texture_view;
        if (!get_next_texture_view(surface, &surface_texture, &texture_view)) {
            return;
        }

        WGPURenderPassColorAttachment render_pass_color_attachment = {};
        render_pass_color_attachment.view = texture_view;
        render_pass_color_attachment.resolveTarget = NULL;
        render_pass_color_attachment.loadOp = WGPULoadOp_Clear;
        render_pass_color_attachment.storeOp = WGPUStoreOp_Store;
        render_pass_color_attachment.depthSlice = WGPU_DEPTH_SLICE_UNDEFINED;
        render_pass_color_attachment.clearValue = WGPUColor{
            clear_color[0], clear_color[1], clear_color[2], 1.0
        };

        WGPURenderPassDescriptor render_pass_desc = {};
        render_pass_desc.nextInChain = NULL;
        render_pass_desc.colorAttachmentCount = 1;
        render_pass_desc.colorAttachments = &render_pass_color_attachment;

        WGPURenderPassEncoder render_pass = wgpuCommandEncoderBeginRenderPass(encoder, &render_pass_desc);
        wgpuRenderPassEncoderSetPipeline(render_pass, pipeline);

        wgpuRenderPassEncoderSetVertexBuffer(render_pass, 0, vertex_buffer, 0, vertex_buffer_size);
        wgpuRenderPassEncoderSetBindGroup(render_pass, 0, bind_group, 0, NULL);
        wgpuRenderPassEncoderDraw(render_pass, 6, 1, 0, 0);

        wgpuRenderPassEncoderSetVertexBuffer(render_pass, 0, vertex_buffer2, 0, vertex_buffer2_size);
        wgpuRenderPassEncoderSetBindGroup(render_pass, 0, bind_group2, 0, NULL);
        wgpuRenderPassEncoderDraw(render_pass, 3, 1, 0, 0);

        ImGui_ImplWGPU_RenderDrawData(ImGui::GetDrawData(), render_pass);

        wgpuRenderPassEncoderEnd(render_pass);
        wgpuRenderPassEncoderRelease(render_pass);

        WGPUCommandBufferDescriptor command_buffer_desc = {};
        command_buffer_desc.nextInChain = NULL;
        WGPUCommandBuffer command_buffer = wgpuCommandEncoderFinish(encoder, &command_buffer_desc);
        wgpuCommandEncoderRelease(encoder);

        wgpuQueueSubmit(queue, 1, &command_buffer);
        wgpuCommandBufferRelease(command_buffer);

        wgpuSurfacePresent(surface);

        wgpuTextureViewRelease(texture_view);
}

void terminate(
    SDL_Window *window,
    SDL_MetalView metal_view,
    WGPUInstance instance,
    WGPUAdapter adapter,
    WGPUDevice device,
    WGPUSurface surface,
    WGPURenderPipeline pipeline,
    WGPUQueue queue)
{
    ImGui_ImplWGPU_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();
    SDL_Metal_DestroyView(metal_view);
    SDL_DestroyWindow(window);
    SDL_Quit();
    wgpuAdapterRelease(adapter);
    wgpuDeviceRelease(device);
    wgpuInstanceRelease(instance);
    wgpuRenderPipelineRelease(pipeline);
}

void gui() {
	ImGui::ColorEdit3("clear color", clear_color);
	ImGui::ColorEdit3("triangle color", triangle_color);
    ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate,
            ImGui::GetIO().Framerate);
}

int main() {
    // TODO: state type
    SDL_Window *window = NULL;
    SDL_MetalView metal_view = NULL;
    WGPUAdapter adapter = NULL;
    WGPUDevice device = NULL;
    WGPUInstance instance = NULL;
    WGPUSurface surface = NULL;
    WGPURenderPipeline pipeline = NULL;
    WGPUQueue queue = NULL;
    WGPUBuffer uniform_buffer = NULL;
    WGPUBindGroup bind_group = NULL;
    WGPUBuffer uniform_buffer2 = NULL;
    WGPUBindGroup bind_group2 = NULL;

    initialize(&window, &metal_view, &instance, &adapter, &device, &surface, &pipeline, &queue, &uniform_buffer, &bind_group, &uniform_buffer2, &bind_group2);

    const float vertices[] = {
        0.0f,  0.6f,   1.0f, 0.0f, 1.0f, 0.0f,
        0.5f,  0.9f,   1.0f, 0.0f, 0.0f, 0.2f,
        -0.5f, 0.8f,   1.0f, 0.0f, 0.0f, 0.2f,
        0.0f,  0.55f,  1.0f, 1.0f, 0.0f, 0.0f,
        0.5f,  0.8f,   1.0f, 1.0f, 1.0f, 0.0f,
        0.5f,  -0.45f, 1.0f, 0.0f, 1.0f, 1.0f
    };

    WGPUBufferDescriptor vertex_buffer_desc = {};
    vertex_buffer_desc.nextInChain = NULL;
    vertex_buffer_desc.usage = WGPUBufferUsage_Vertex | WGPUBufferUsage_CopyDst;
    vertex_buffer_desc.size = sizeof(vertices);
    vertex_buffer_desc.mappedAtCreation = false;
    WGPUBuffer vertex_buffer = wgpuDeviceCreateBuffer(device, &vertex_buffer_desc);

    const float vertices2[] = {
        0.0f,  0.5f,  1.0f, 1.0f, 1.0f, 1.0f,
        -0.5f, -0.5f, 1.0f, 1.0f, 1.0f, 1.0f,
        0.5f,  -0.5f, 1.0f, 1.0f, 1.0f, 1.0f
    };

    WGPUBufferDescriptor vertex_buffer2_desc = {};
    vertex_buffer2_desc.nextInChain = NULL;
    vertex_buffer2_desc.usage = WGPUBufferUsage_Vertex | WGPUBufferUsage_CopyDst;
    vertex_buffer2_desc.size = sizeof(vertices2);
    vertex_buffer2_desc.mappedAtCreation = false;
    WGPUBuffer vertex_buffer2 = wgpuDeviceCreateBuffer(device, &vertex_buffer2_desc);

    wgpuQueueWriteBuffer(queue, vertex_buffer, 0, vertices, sizeof(vertices));
    wgpuQueueWriteBuffer(queue, vertex_buffer2, 0, vertices2, sizeof(vertices2));
    wgpuQueueWriteBuffer(queue, uniform_buffer, 0, &triangle_color, sizeof(triangle_color));

    bool running = true;
    while (running) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_EVENT_QUIT) running = false;
            ImGui_ImplSDL3_ProcessEvent(&e);
        }

        ImGui_ImplSDL3_NewFrame();
        ImGui_ImplWGPU_NewFrame();
        ImGui::NewFrame();
        gui();
        ImGui::Render();
        wgpuQueueWriteBuffer(queue, uniform_buffer2, 0, &triangle_color, sizeof(triangle_color));

        render(device, surface, pipeline, queue, vertex_buffer, sizeof(vertices), vertex_buffer2, sizeof(vertices2), bind_group, bind_group2);
    }

    terminate(window, metal_view, instance, adapter, device, surface, pipeline, queue);
}
