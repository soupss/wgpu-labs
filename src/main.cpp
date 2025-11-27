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
#include "camera.h"

typedef struct {
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

void _update(State *s, bool *running) {
    SDL_Event e;

    const float sens_rot = 0.0015;
    const float sens_zoom = 2.0;
    while (SDL_PollEvent(&e)) {
        if (e.type == SDL_EVENT_QUIT || (e.type == SDL_EVENT_KEY_DOWN && e.key.key == SDLK_ESCAPE)) {
            *running = false;
        }

        if (e.type == SDL_EVENT_MOUSE_WHEEL) {
            int dir;
            if (e.wheel.y > 0) dir = 1;
            if (e.wheel.y < 0) dir = -1;
            s->camera.distance += dir * sens_zoom;
        }

        if (e.type == SDL_EVENT_MOUSE_MOTION) {
            s->camera.yaw += e.motion.xrel * sens_rot;
            s->camera.pitch -= e.motion.yrel * sens_rot;
        }
    }
    const float sens_move = 0.5;
    float forward = 0, right = 0, up = 0;
    const bool *keys = SDL_GetKeyboardState(NULL);
    float dir[3];
    if (keys[SDL_SCANCODE_W]) {
        forward = sens_move;
    }
    if (keys[SDL_SCANCODE_A]) {
        right = -sens_move;
    }
    if (keys[SDL_SCANCODE_S]) {
        forward = -sens_move;
    }
    if (keys[SDL_SCANCODE_D]) {
        right = sens_move;
    }
    if (keys[SDL_SCANCODE_SPACE]) {
        up = sens_move;
    }
    if (keys[SDL_SCANCODE_LCTRL]) {
        up = -sens_move;
    }

    ImGui_ImplSDL3_ProcessEvent(&e);

    camera_move(&s->camera, forward, right, up);

    UBOData_Frame ubo_data_frame = {0};
    camera_get_view_projection(&s->camera, ubo_data_frame.view_projection);

    UBOData_Object ubo_data_car = {
        .model = GLM_MAT4_IDENTITY_INIT
    };
    glm_translate(ubo_data_car.model, (vec3){0.0, 5.0, 0.0});

    UBOData_Object ubo_data_city = {
        .model = GLM_MAT4_IDENTITY_INIT
    };
    glm_translate(ubo_data_city.model, (vec3){0.0, 0.0, 0.0});

    uint64_t freq = SDL_GetPerformanceFrequency();
    ubo_data_frame.time = (float)(SDL_GetPerformanceCounter() / (float)freq);

    wgpuQueueWriteBuffer(s->queue, s->ubo_frame, 0, &ubo_data_frame, sizeof(UBOData_Frame));
    wgpuQueueWriteBuffer(s->queue, s->ubo_object, 0, &ubo_data_car, sizeof(UBOData_Object));
    wgpuQueueWriteBuffer(s->queue, s->ubo_object, UBO_OBJECT_SLOT_SIZE, &ubo_data_city, sizeof(UBOData_Object));
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
        .clearValue = WGPUColor{ 0.1, 0.1, 0.1, 1.0 }
    };

    WGPUTextureDescriptor depth_desc = {
        .usage = WGPUTextureUsage_RenderAttachment,
        .size = { WINDOW_WIDTH, WINDOW_HEIGHT, 1 },
        .format = WGPUTextureFormat_Depth24Plus,
        .mipLevelCount = 1,
        .sampleCount = 1,
        .dimension = WGPUTextureDimension_2D,
    };
    WGPUTexture depthTex = wgpuDeviceCreateTexture(s->device, &depth_desc);
    WGPUTextureView depthView = wgpuTextureCreateView(depthTex, NULL);

    WGPURenderPassDepthStencilAttachment ds_att = {
        .view = depthView,
        .depthLoadOp = WGPULoadOp_Clear,
        .depthStoreOp = WGPUStoreOp_Store,
        .depthClearValue = 1.0f,
    };

    WGPURenderPassDescriptor render_pass_desc = {
        .nextInChain = NULL,
        .colorAttachmentCount = 1,
        .colorAttachments = &render_pass_color_attachment,
        .depthStencilAttachment = &ds_att
    };


    // begin render pass
    WGPURenderPassEncoder render_pass = wgpuCommandEncoderBeginRenderPass(encoder, &render_pass_desc);

    wgpuRenderPassEncoderSetPipeline(render_pass, s->pipeline);

    uint32_t offset = 0;
    wgpuRenderPassEncoderSetBindGroup(render_pass, 0, s->bg, 1, &offset);
    model_render(&s->model_car, render_pass);

    offset = UBO_OBJECT_SLOT_SIZE;
    wgpuRenderPassEncoderSetBindGroup(render_pass, 0, s->bg, 1, &offset);
    model_render(&s->model_city, render_pass);

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
    wgpuTextureViewRelease(depthView);
    wgpuTextureRelease(depthTex);
}

void _terminate(State *s) {
    ImGui_ImplWGPU_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();

    SDL_Metal_DestroyView(s->metal_view);
    SDL_DestroyWindow(s->window);
    SDL_Quit();

    wgpuSurfaceRelease(s->surface);
    wgpuBindGroupRelease(s->bg);
    wgpuAdapterRelease(s->adapter);
    wgpuDeviceRelease(s->device);
    wgpuInstanceRelease(s->instance);
    wgpuRenderPipelineRelease(s->pipeline);
}

int main() {
    State s = {
        .camera = {
            .pos = {15, 15, 15},
            .target = {-1, -1, -1},
            .yaw = 0.0,
            .pitch = 0.0,
            .distance = 25.0
        }
    };

    Options o = {
        .camera_pan = 0.0,
        .mag_filter = WGPUFilterMode_Linear,
        .min_filter = WGPUFilterMode_Linear,
        .mipmap_filter = WGPUMipmapFilterMode_Linear,
        .max_anisotropy = 1,
    };

    initialize(&s);

    bool running = true;
    while (running) {
        _update(&s, &running);

        _render_imgui(&o);

        _render(&s);
    }

    _terminate(&s);
}
