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

const float THREE_TRIANGLES[] = {
    // Triangle 1
    0.0f, 0.5f, 1.0f, 1.0f, 0.0f, 1.0f,
    -0.5f, -0.5f, 1.0f, 0.0f, 1.0f, 1.0f,
    0.5f, -0.5f, 1.0f, 1.0f, 1.0f, 0.0f,

    // Triangle 2
    0.1f, 0.5f, 1.0f, 1.0f, 0.0f, 1.0f,
    0.5f, -0.5f, 1.0f, 1.0f, 1.0f, 0.0f,
    0.5f, 0.5f, 1.0f, 0.0f, 1.0f, 1.0f,

    // Triangle 3
    -0.1f, 0.5f, 1.0f, 1.0f, 0.0f, 1.0f,
    -0.5f, -0.5f, 1.0f, 0.0f, 1.0f, 1.0f,
    -0.5f, 0.5f, 1.0f, 1.0f, 1.0f, 0.0f};

void initialize(
    SDL_Window **window,
    WGPUDevice *device,
    WGPUInstance *instance,
    WGPUSurface *surface,
    WGPUQueue *queue,
    WGPURenderPipeline *pipeline,
    WGPUPipelineLayout *pipeline_layout,
    WGPUBindGroupLayout *bind_group_layout)
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

    unsigned long vertex_buffer_size = sizeof(THREE_TRIANGLES) + 4;
    WGPUBuffer vertex_buffer = create_buffer(device, vertex_buffer_size, WGPUBufferUsage_CopyDst | WGPUBufferUsage_Vertex);
    wgpuQueueWriteBuffer(queue, vertex_buffer, 0, THREE_TRIANGLES, vertex_buffer_size);

    const float inital_color[] = {0.0f, 1.0f, 0.0f};
    WGPUBuffer color_buffer = create_buffer(device, sizeof(inital_color) + 4, WGPUBufferUsage_CopyDst | WGPUBufferUsage_Uniform);

    wgpuQueueWriteBuffer(queue, color_buffer, 0, inital_color, sizeof(inital_color));

    WGPUBindGroup bindGroup = create_bind_group(device, bind_group_layout, color_buffer, 0, 4 * sizeof(float));

    const float white[] = {1.0f, 1.0f, 1.0f};
    WGPUBuffer white_buffer = create_buffer(device, sizeof(white) + 4, WGPUBufferUsage_CopyDst | WGPUBufferUsage_Uniform);

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
            else if (e.type == SDL_EVENT_KEY_DOWN)
            {
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
