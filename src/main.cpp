#include <stdio.h>
#include <webgpu.h>
#include <SDL3/SDL.h>
#include <utility>

#include <iostream>
#include <cassert>
#include <vector>

#include "init.cpp"

#define WINDOW_WIDTH 800
#define WINDOW_HEIGHT 600

// Structure to hold render pass state between begin and end
typedef struct RenderPassState {
    WGPUCommandEncoder encoder;
    WGPURenderPassEncoder renderPass;
    WGPUQueue queue;
} RenderPassState;


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

void configure_surface(WGPUSurface *surface, WGPUDevice *device, WGPUAdapter adapter,WGPUSurfaceCapabilities surface_capabilities)
{
    // configure surface
    WGPUSurfaceConfiguration surface_config = {0};
    surface_config.nextInChain = NULL;
    surface_config.width = WINDOW_WIDTH;
    surface_config.height = WINDOW_HEIGHT;
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

RenderPassState begin_render_pass(WGPUDevice device, WGPUTextureView targetView)
{
    RenderPassState state = {};
    
    state.queue = wgpuDeviceGetQueue(device);

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

    state.encoder = wgpuDeviceCreateCommandEncoder(device, &encoderDesc);
    state.renderPass = wgpuCommandEncoderBeginRenderPass(state.encoder, &renderPassDesc);
    
    return state;
}

void end_render_pass(RenderPassState state)
{
    wgpuRenderPassEncoderEnd(state.renderPass);

    WGPUCommandBufferDescriptor cmdBufferDescriptor = {};
    cmdBufferDescriptor.nextInChain = NULL;

    WGPUCommandBuffer command = wgpuCommandEncoderFinish(state.encoder, &cmdBufferDescriptor);
    wgpuCommandEncoderRelease(state.encoder); // release encoder after it's finished

    wgpuQueueSubmit(state.queue, 1, &command);
    wgpuCommandBufferRelease(command);
}

WGPUVertexBufferLayout * get_vertex_buffer_layouts() {
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

    WGPUVertexBufferLayout* out = (WGPUVertexBufferLayout*) malloc(sizeof(WGPUVertexBufferLayout));
    *out = vertex_buffer_layout;
    return out;

}

void initialize(
    SDL_Window **window,
    WGPUDevice *device,
    WGPUInstance *instance,
    WGPUSurface *surface,
    WGPUQueue *queue,
    WGPURenderPipeline *pipeline,
WGPUPipelineLayout * pipeline_layout,
WGPUBindGroupLayout * bind_group_layout
)
{

    // Initialize components in proper order
    void *metal_layer = NULL;
    initialize_window(window, &metal_layer);
    initialize_instance(instance);
    initialize_surface(instance, metal_layer, surface);

    WGPUAdapter adapter = NULL;
    initialize_device(instance, device, surface, &adapter);
    
    WGPUSurfaceCapabilities surface_capabilities = {0};
    wgpuSurfaceGetCapabilities(*surface, adapter, &surface_capabilities);
    configure_surface(surface, device, adapter, surface_capabilities);
    
    // Release adapter after use
    wgpuAdapterRelease(adapter);

    initialize_layout(device, pipeline_layout, bind_group_layout);

    initialize_pipeline(pipeline, device, pipeline_layout, surface_capabilities);

    *queue = wgpuDeviceGetQueue(*device);
}

WGPUBindGroup create_bind_group(WGPUDevice device, WGPUBindGroupLayout bind_group_layout, WGPUBuffer buffer, uint32_t binding_index, size_t buffer_size) {
    // Create a binding
    WGPUBindGroupEntry binding = {};
    // The index of the binding (the entries in bindGroupDesc can be in any order)
    binding.binding = binding_index;
    // The buffer it is actually bound to
    binding.buffer = buffer;
    // We can specify an offset within the buffer, so that a single buffer can hold
    // multiple uniform blocks.
    binding.offset = 0;
    // And we specify again the size of the buffer.
    binding.size = buffer_size;

    // A bind group contains one or multiple bindings
    WGPUBindGroupDescriptor bindGroupDesc = {};
    bindGroupDesc.layout = bind_group_layout;
    // There must be as many bindings as declared in the layout!
    bindGroupDesc.entryCount = 1;
    bindGroupDesc.entries = &binding;
    return wgpuDeviceCreateBindGroup(device, &bindGroupDesc);
}

WGPUBuffer create_uniform_buffer(WGPUDevice device, size_t buffer_size) {
    WGPUBufferDescriptor buffer_desc = {};
    buffer_desc.nextInChain = NULL;
    buffer_desc.usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_Uniform;
    buffer_desc.size = buffer_size;
    buffer_desc.mappedAtCreation = false;
    return wgpuDeviceCreateBuffer(device, &buffer_desc);
}

int main()
{
    SDL_Window *window = NULL;
    WGPUDevice device = NULL;
    WGPUInstance instance = NULL;
    WGPUSurface surface = NULL;
    WGPUQueue queue = NULL;
    WGPURenderPipeline pipeline = NULL;
    WGPUPipelineLayout pipeline_layout = NULL;
    WGPUBindGroupLayout bind_group_layout = NULL;

    initialize(&window, &device, &instance, &surface, &queue, &pipeline, &pipeline_layout, &bind_group_layout);

    const float vertices[] = {
        //Triangle 1
		0.0f,  0.5f,  1.0f, 1.0f, 0.0f, 1.0f,
		-0.5f, -0.5f, 1.0f, 0.0f, 1.0f, 1.0f,
		0.5f,  -0.5f, 1.0f, 1.0f, 1.0f, 0.0f,
        
        //Triangle 2
		0.1f,  0.5f,  1.0f, 1.0f, 0.0f, 1.0f,
		0.5f, -0.5f, 1.0f, 1.0f, 1.0f, 0.0f,
		0.5f,  0.5f, 1.0f, 0.0f, 1.0f, 1.0f,
        
        //Triangle 3
		-0.1f,  0.5f,  1.0f, 1.0f, 0.0f, 1.0f,
		-0.5f, -0.5f, 1.0f, 0.0f, 1.0f, 1.0f,
		-0.5f,  0.5f, 1.0f, 1.0f, 1.0f, 0.0f
    };
    
    unsigned long vertex_buffer_size = sizeof(vertices);
    WGPUBufferDescriptor vertex_buffer_desc = {};
    vertex_buffer_desc.nextInChain = NULL;
    vertex_buffer_desc.usage = WGPUBufferUsage_Vertex | WGPUBufferUsage_CopyDst;
    vertex_buffer_desc.size = vertex_buffer_size;
    vertex_buffer_desc.mappedAtCreation = false;
    WGPUBuffer vertex_buffer = wgpuDeviceCreateBuffer(device, &vertex_buffer_desc);
    
    wgpuQueueWriteBuffer(queue, vertex_buffer, 0, vertices, vertex_buffer_size);
    
    const float inital_color[] = {0.0f, 1.0f, 0.0f};
    WGPUBuffer color_buffer = create_uniform_buffer(device, sizeof(inital_color) + 4);

    wgpuQueueWriteBuffer(queue, color_buffer, 0, inital_color, sizeof(inital_color));

    WGPUBindGroup bindGroup = create_bind_group(device, bind_group_layout, color_buffer, 0, 4 * sizeof(float));

    const float white[] = {1.0f, 1.0f, 1.0f};
    WGPUBuffer white_buffer = create_uniform_buffer(device, sizeof(white) + 4);

    wgpuQueueWriteBuffer(queue, white_buffer, 0, white, sizeof(white));
    
    WGPUBindGroup bindGroup2 = create_bind_group(device, bind_group_layout, white_buffer, 0, 4 * sizeof(float));
    // ...existing code...

    bool running = true;
    while (running)
    {
        SDL_Event e;
        while (SDL_PollEvent(&e))
        {
            if (e.type == SDL_EVENT_QUIT)
                running = false;
            else if (e.type == SDL_EVENT_KEY_DOWN) {

            }
        }
        WGPUTextureView targetView = NULL;
        WGPUSurfaceTexture surfaceTexture;
        get_next_surface_texture_view(&surface, &targetView, &surfaceTexture);

        // Begin render pass
        RenderPassState renderState = begin_render_pass(device, targetView);

        wgpuRenderPassEncoderSetVertexBuffer(renderState.renderPass, 0, vertex_buffer, 0, vertex_buffer_size);
        wgpuRenderPassEncoderSetPipeline(renderState.renderPass, pipeline);
        // Add this line to set the bind group
        
        wgpuRenderPassEncoderSetBindGroup(renderState.renderPass, 0, bindGroup, 0, NULL);
        wgpuRenderPassEncoderDraw(renderState.renderPass, 6, 1, 0, 0);
        wgpuRenderPassEncoderSetBindGroup(renderState.renderPass, 0, bindGroup2, 0, NULL);
        wgpuRenderPassEncoderDraw(renderState.renderPass, 3, 1, 6, 0);
        
        // End render pass
        end_render_pass(renderState);

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
