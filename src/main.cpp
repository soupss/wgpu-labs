#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <webgpu.h>
#include <SDL3/SDL.h>
#include <cglm/cglm.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_wgpu.h>
#include "constants.h"
#include "init.hpp"
#include "state.h"

typedef struct Options {
    float camera_pan;
    int mag_filter;
    int min_filter;
    int mipmap_filter;
    int max_anisotropy;
} Options;

void _render_imgui(Options *o) {
    ImGui_ImplSDL3_NewFrame();
    ImGui_ImplWGPU_NewFrame();
    ImGui::NewFrame();

    ImGui::Render();
}

void _render(State *s) {
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
    wgpuRenderPassEncoderSetIndexBuffer(render_pass,
            s->index_buffer,
            WGPUIndexFormat_Uint32,
            0,
            s->mesh_car.index_count * sizeof(int));
    wgpuRenderPassEncoderSetBindGroup(render_pass, 0, s->bg_asphalt, 0, NULL);
    wgpuRenderPassEncoderDrawIndexed(render_pass, s->mesh_car.index_count, 1, 0, 0, 0);

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

void _terminate(State *s) {
    ImGui_ImplWGPU_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();

    SDL_Metal_DestroyView(s->metal_view);
    SDL_DestroyWindow(s->window);
    SDL_Quit();

    wgpuSurfaceRelease(s->surface);
    wgpuBufferRelease(s->vertex_buffer);
    wgpuBufferRelease(s->uniform_buffer);
    wgpuBindGroupLayoutRelease(s->bgl);
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
        .mesh_car = NULL,
        .texture_asphalt = NULL,
        .bg_asphalt = NULL,
        .bg_explosion = NULL
    };

    Options o = {
        .camera_pan = 0.0,
        .mag_filter = WGPUFilterMode_Linear,
        .min_filter = WGPUFilterMode_Linear,
        .mipmap_filter = WGPUMipmapFilterMode_Linear,
        .max_anisotropy = 1,
    };

    initialize(&s);


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
        vec3 camera_position = {o.camera_pan, 0, 20};
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

        _render_imgui(&o);

        _render(&s);
    }

    _terminate(&s);
}
