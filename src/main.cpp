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
    WGPUTextureView texture_view = wgpuTextureCreateView(surface_texture.texture, &view_desc);

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

    wgpuRenderPassEncoderSetVertexBuffer(render_pass, 0, s->vbo_car, 0, WGPU_WHOLE_SIZE);
    wgpuRenderPassEncoderSetIndexBuffer(render_pass,
            s->ibo_car,
            WGPUIndexFormat_Uint32,
            0,
            s->mesh_car.index_count * sizeof(int));
    unsigned int offset = 0;
    wgpuRenderPassEncoderSetBindGroup(render_pass, 0, s->bg, 1, &offset);
    wgpuRenderPassEncoderDrawIndexed(render_pass, s->mesh_car.index_count, 1, 0, 0, 0);

    wgpuRenderPassEncoderSetVertexBuffer(render_pass, 0, s->vbo_city, 0, WGPU_WHOLE_SIZE);
    wgpuRenderPassEncoderSetIndexBuffer(render_pass,
            s->ibo_city,
            WGPUIndexFormat_Uint32,
            0,
            s->mesh_city.index_count * sizeof(int));
    offset = UBO_OBJECT_SLOT_SIZE;

    wgpuRenderPassEncoderSetBindGroup(render_pass, 0, s->bg, 1, &offset);
    wgpuRenderPassEncoderDrawIndexed(render_pass, s->mesh_city.index_count, 1, 0, 0, 0);

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
    wgpuBufferRelease(s->vbo_car);
    wgpuBindGroupRelease(s->bg);
    wgpuAdapterRelease(s->adapter);
    wgpuDeviceRelease(s->device);
    wgpuInstanceRelease(s->instance);
    wgpuRenderPipelineRelease(s->pipeline);
}

int main() {
    State s = {0};

    Options o = {
        .camera_pan = 0.0,
        .mag_filter = WGPUFilterMode_Linear,
        .min_filter = WGPUFilterMode_Linear,
        .mipmap_filter = WGPUMipmapFilterMode_Linear,
        .max_anisotropy = 1,
    };

    initialize(&s);

    UBOData_Frame ubo_data_frame = {0};

    mat4 view = GLM_MAT4_IDENTITY_INIT;
    vec3 camera_pos = {15.0f, 15.0f, 15.0f};
    vec3 camera_dir = {-1.0f, -1.0f, -1.0f};
    vec3 up = {0.0f, 1.0f, 0.0f};
    glm_lookat(camera_pos, camera_dir, up, view);

    mat4 projection = GLM_MAT4_IDENTITY_INIT;
    float fovy = 45.0;
    float aspect_ratio = (float)WINDOW_WIDTH / (float)WINDOW_HEIGHT;
    float near_plane = 0.01f;
    float far_plane = 300.0f;
    glm_perspective(fovy, aspect_ratio, near_plane, far_plane, projection);

    glm_mat4_mul(projection, view, ubo_data_frame.view_projection);

    UBOData_Object ubo_data_car = {
        .model = GLM_MAT4_IDENTITY_INIT
    };
    glm_translate(ubo_data_car.model, (vec3){0.0, 5.0, 0.0});

    UBOData_Object ubo_data_city = {
        .model = GLM_MAT4_IDENTITY_INIT
    };
    glm_translate(ubo_data_city.model, (vec3){0.0, 0.0, 0.0});

    uint64_t freq = SDL_GetPerformanceFrequency();
    bool running = true;
    while (running) {
        // events
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_EVENT_QUIT) running = false;
            ImGui_ImplSDL3_ProcessEvent(&e);
        }
        // calculations

        ubo_data_frame.time = (float)(SDL_GetPerformanceCounter() / (float)freq);

        wgpuQueueWriteBuffer(s.queue, s.ubo_frame, 0, &ubo_data_frame, sizeof(UBOData_Frame));
        wgpuQueueWriteBuffer(s.queue, s.ubo_object, 0, &ubo_data_car, sizeof(UBOData_Object));
        wgpuQueueWriteBuffer(s.queue, s.ubo_object, UBO_OBJECT_SLOT_SIZE, &ubo_data_city, sizeof(UBOData_Object));

        // render

        _render_imgui(&o);

        _render(&s);
    }

    _terminate(&s);
}
