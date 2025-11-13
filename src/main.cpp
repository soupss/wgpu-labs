#include <stdio.h>
#include <webgpu.h>
#include <SDL3/SDL.h>
#include <utility>

#include <iostream>
#include <cassert>
#include <vector>

#define WINDOW_WIDTH 800
#define WINDOW_HEIGHT 600
#define WEBGPU_STR(str) (WGPUStringView){.data = str, .length = sizeof(str) - 1}
// We embbed the source of the shader module here
const char *shaderSource = R"(
@vertex
fn vs_main(@builtin(vertex_index) in_vertex_index: u32) -> @builtin(position) vec4f {
	var p = vec2f(0.0, 0.0);
	if (in_vertex_index == 0u) {
		p = vec2f(-0.5, -0.5);
	} else if (in_vertex_index == 1u) {
		p = vec2f(0.5, -0.5);
	} else {
		p = vec2f(0.0, 0.5);
	}
	return vec4f(p, 0.0, 1.0);
}

@fragment
fn fs_main() -> @location(0) vec4f {
	return vec4f(0.0, 0.4, 1.0, 1.0);
}
)";

typedef struct AdapterRequestState
{
    WGPUAdapter adapter = NULL;
    bool request_ended = false;
} AdapterRequestState;

void print_adapter_info(WGPUAdapter adapter)
{
    // Adapter info
    WGPUAdapterInfo info = {0};
    if (wgpuAdapterGetInfo(adapter, &info) == WGPUStatus_Success)
    {
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
    WGPUSupportedFeatures features = {0};
    wgpuAdapterGetFeatures(adapter, &features);
    printf("\n=== Supported Features (%zu) ===\n", features.featureCount);
    for (size_t i = 0; i < features.featureCount; ++i)
    {
        printf("  - Feature #%zu: %d\n", i, features.features[i]);
    }
    wgpuSupportedFeaturesFreeMembers(features);

    // Adapter limits
    WGPULimits limits = {0};
    if (wgpuAdapterGetLimits(adapter, &limits) == WGPUStatus_Success)
    {
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

typedef struct DeviceRequestState
{
    WGPUDevice *device = NULL;
    bool request_ended = false;
} DeviceRequestState;

void print_device_info(WGPUDevice device)
{
    // --- Device features
    WGPUSupportedFeatures dfeatures = (WGPUSupportedFeatures){0};
    wgpuDeviceGetFeatures(device, &dfeatures);
    printf("\n=== Device Features (%zu) ===\n", dfeatures.featureCount);
    for (size_t i = 0; i < dfeatures.featureCount; ++i)
    {
        printf("  - %d\n", dfeatures.features[i]); // WGPUFeatureName
    }
    wgpuSupportedFeaturesFreeMembers(dfeatures);

    // --- Device limits
    WGPULimits dlimits = (WGPULimits){0};
    if (wgpuDeviceGetLimits(device, &dlimits) == WGPUStatus_Success)
    {
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

void get_next_surface_texture_view(WGPUSurface *surface, WGPUTextureView *targetView, WGPUSurfaceTexture *surfaceTexture)
{
    wgpuSurfaceGetCurrentTexture(*surface, surfaceTexture);
    if (surfaceTexture->status != WGPUSurfaceGetCurrentTextureStatus_SuccessOptimal && surfaceTexture->status != WGPUSurfaceGetCurrentTextureStatus_SuccessSuboptimal)
    {
        return;
    }

    WGPUTextureViewDescriptor viewDescriptor;
    viewDescriptor.nextInChain = NULL;
    viewDescriptor.format = wgpuTextureGetFormat(surfaceTexture->texture);
    viewDescriptor.dimension = WGPUTextureViewDimension_2D;
    viewDescriptor.baseMipLevel = 0;
    viewDescriptor.mipLevelCount = 1;
    viewDescriptor.baseArrayLayer = 0;
    viewDescriptor.arrayLayerCount = 1;
    viewDescriptor.aspect = WGPUTextureAspect_All;
    viewDescriptor.usage = WGPUTextureUsage_RenderAttachment;
    *targetView = wgpuTextureCreateView(surfaceTexture->texture, &viewDescriptor);
}

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

void configure_surface(WGPUSurface *surface, WGPUDevice *device, WGPUAdapter adapter)
{
    // configure surface
    WGPUSurfaceConfiguration surface_config = {0};
    surface_config.nextInChain = NULL;
    surface_config.width = WINDOW_WIDTH;
    surface_config.height = WINDOW_HEIGHT;
    WGPUSurfaceCapabilities surface_capabilities = {0};
    wgpuSurfaceGetCapabilities(*surface, adapter, &surface_capabilities);
    WGPUTextureFormat surface_format = WGPUTextureFormat_Undefined;
    if (surface_capabilities.formatCount > 0)
    {
        surface_format = surface_capabilities.formats[0];
    }
    surface_config.format = surface_format;
    surface_config.viewFormatCount = 0;
    surface_config.viewFormats = NULL;
    surface_config.usage = WGPUTextureUsage_RenderAttachment;
    surface_config.device = *device;
    surface_config.presentMode = WGPUPresentMode_Fifo;
    surface_config.alphaMode = WGPUCompositeAlphaMode_Auto;
    wgpuSurfaceConfigure(*surface, &surface_config);
}

void render_pass_type_shit(WGPUDevice device, WGPUTextureView targetView)
{
    WGPUQueue queue = wgpuDeviceGetQueue(device);

    WGPURenderPassDescriptor renderPassDesc = {};
    renderPassDesc.nextInChain = NULL;
    WGPUCommandEncoderDescriptor encoderDesc = {};
    encoderDesc.nextInChain = NULL;

    WGPURenderPassColorAttachment renderPassColorAttachment = {};
    renderPassColorAttachment.view = targetView;
    renderPassColorAttachment.resolveTarget = NULL;
    renderPassColorAttachment.loadOp = WGPULoadOp_Clear;
    renderPassColorAttachment.storeOp = WGPUStoreOp_Store;
    renderPassColorAttachment.depthSlice = WGPU_DEPTH_SLICE_UNDEFINED;
    renderPassColorAttachment.clearValue = WGPUColor{0.9, 0.1, 0.2, 1.0};

    renderPassDesc.colorAttachmentCount = 1;
    renderPassDesc.colorAttachments = &renderPassColorAttachment;

    WGPUCommandEncoder encoder = wgpuDeviceCreateCommandEncoder(device, &encoderDesc);

    //
    // RENDER PASSS
    //
    WGPURenderPassEncoder renderPass = wgpuCommandEncoderBeginRenderPass(encoder, &renderPassDesc);
    wgpuRenderPassEncoderEnd(renderPass);
    //
    // RENDER PASSS
    //

    WGPUCommandBufferDescriptor cmdBufferDescriptor = {};
    cmdBufferDescriptor.nextInChain = NULL;

    WGPUCommandBuffer command = wgpuCommandEncoderFinish(encoder, &cmdBufferDescriptor);
    wgpuCommandEncoderRelease(encoder); // release encoder after it's finished

    wgpuQueueSubmit(queue, 1, &command);
    wgpuCommandBufferRelease(command);
}

void initialize_pipeline(WGPURenderPipeline *pipeline, WGPUDevice *device)
{

    WGPUShaderModuleDescriptor shaderDesc{};

    
    WGPUShaderModule shaderModule = wgpuDeviceCreateShaderModule(*device, &shaderDesc);

    WGPURenderPipelineDescriptor pipelineDesc{};
    pipelineDesc.nextInChain = NULL;


    // [...] Describe vertex pipeline state
    pipelineDesc.vertex.bufferCount = 0;
    pipelineDesc.vertex.buffers = NULL;
    pipelineDesc.vertex.module = shaderModule;
    pipelineDesc.vertex.entryPoint = WEBGPU_STR("vs_main");
    pipelineDesc.vertex.constantCount = 0;
    pipelineDesc.vertex.constants = NULL;


    // [...] Describe primitive pipeline state
    pipelineDesc.primitive.topology = WGPUPrimitiveTopology_TriangleList;
    pipelineDesc.primitive.stripIndexFormat = WGPUIndexFormat_Undefined;
    pipelineDesc.primitive.frontFace = WGPUFrontFace_CCW;
    pipelineDesc.primitive.cullMode = WGPUCullMode_None;
    
    // [...] Describe stencil/depth pipeline state
    pipelineDesc.depthStencil = NULL;
    
    // [...] Describe fragment pipeline state
    WGPUFragmentState fragmentState{};
    fragmentState.module = shaderModule;
    fragmentState.entryPoint = WEBGPU_STR("fs_main");
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

    WGPUColorTargetState colorTarget{};
    colorTarget.format = WGPUTextureFormat_Undefined;
    colorTarget.blend = &blendState;
    colorTarget.writeMask = WGPUColorWriteMask_All; // We could write to only some of the color channels.

    fragmentState.targetCount = 1;
    fragmentState.targets = &colorTarget;


    pipelineDesc.multisample.count = 1;
    // Default value for the mask, meaning "all bits on"
    pipelineDesc.multisample.mask = ~0u;
    // Default value as well (irrelevant for count = 1 anyways)
    pipelineDesc.multisample.alphaToCoverageEnabled = false;

    *pipeline = wgpuDeviceCreateRenderPipeline(*device, &pipelineDesc);
}

void initialize(
    SDL_Window **window,
    WGPUDevice *device,
    WGPUInstance *instance,
    WGPUSurface *surface,
    WGPUQueue *queue,
    WGPURenderPipeline *pipeline)
{
    // Initialize components in proper order
    void *metal_layer = NULL;
    initialize_window(window, &metal_layer);
    initialize_instance(instance);
    initialize_surface(instance, metal_layer, surface);

    WGPUAdapter adapter = NULL;
    initialize_device(instance, device, surface, &adapter);
    configure_surface(surface, device, adapter);

    // Release adapter after use
    wgpuAdapterRelease(adapter);

    initialize_pipeline(pipeline, device);
}

int main()
{
    SDL_Window *window = NULL;
    WGPUDevice device = NULL;
    WGPUInstance instance = NULL;
    WGPUSurface surface = NULL;
    WGPUQueue queue = NULL;
    WGPURenderPipeline pipeline = NULL;

    initialize(&window, &device, &instance, &surface, &queue, &pipeline);

    bool running = true;
    while (running)
    {
        SDL_Event e;
        while (SDL_PollEvent(&e))
        {
            if (e.type == SDL_EVENT_QUIT)
                running = false;
        }
        WGPUTextureView targetView = NULL;
        WGPUSurfaceTexture surfaceTexture;
        get_next_surface_texture_view(&surface, &targetView, &surfaceTexture);

        render_pass_type_shit(device, targetView);

        wgpuSurfacePresent(surface);
        // Release texture after presenting surface
        wgpuTextureRelease(surfaceTexture.texture);

        wgpuTextureViewRelease(targetView);
    }

    // TODO: unconfigure surface
    SDL_DestroyWindow(window);
    SDL_Quit();
    wgpuDeviceRelease(device);
    wgpuInstanceRelease(instance);
}
