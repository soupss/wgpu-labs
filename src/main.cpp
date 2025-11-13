#include <stdio.h>
#include <webgpu.h>
#include <SDL3/SDL.h>
#include <utility>

#include <iostream>
#include <cassert>
#include <vector>

#include "init.cpp"
#include "utils/render.cpp"
#include "utils/webgpu.cpp"

#include "constants.cpp"
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
