#include <stdio.h>
#include <webgpu.h>
#include <SDL3/SDL.h>
#include <utility>

#include <iostream>
#include <cassert>
#include <vector>

#include "../constants.cpp"
// Structure to hold render pass state between begin and end
typedef struct RenderPassState {
    WGPUCommandEncoder encoder;
    WGPURenderPassEncoder renderPass;
    WGPUQueue queue;
} RenderPassState;

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