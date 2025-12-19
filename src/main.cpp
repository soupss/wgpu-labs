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
#include "bind_group.h"
#include "debug.h"

static void _update(State *s, Options *o, bool *running) {
    ImGuiIO &io = ImGui::GetIO();
    SDL_Event e;

    static bool dragging = false;
    static bool third_person = false;
    while (SDL_PollEvent(&e)) {
        ImGui_ImplSDL3_ProcessEvent(&e);
        if (e.type == SDL_EVENT_QUIT || (e.type == SDL_EVENT_KEY_DOWN && e.key.key == SDLK_ESCAPE)) {
            *running = false;
        }

        if (e.type == SDL_EVENT_MOUSE_WHEEL) {
            int dir;
            if (e.wheel.y > 0) dir = 1;
            if (e.wheel.y < 0) dir = -1;
            s->camera.distance += dir * SPEED_CAMERA_ZOOM;
        }

        if (!io.WantCaptureMouse) {
            if (e.type == SDL_EVENT_MOUSE_BUTTON_DOWN) dragging = true;
            if (e.type == SDL_EVENT_MOUSE_BUTTON_UP) dragging = false;
            if (dragging && e.type == SDL_EVENT_MOUSE_MOTION) {
                s->camera.yaw -= e.motion.xrel * SPEED_CAMERA_ROTATE;
                s->camera.pitch += e.motion.yrel * SPEED_CAMERA_ROTATE;
            }
        }

        if (e.type == SDL_EVENT_KEY_DOWN && e.key.key == SDLK_3) {
            third_person = !third_person;
        }
    }

    const bool *keys = SDL_GetKeyboardState(NULL);
    if (!third_person) {
        float forward = 0, right = 0, up = 0;
        // camera controls
        if (keys[SDL_SCANCODE_W]) {
            forward = SPEED_CAMERA_MOVE;
        }
        if (keys[SDL_SCANCODE_A]) {
            right = -SPEED_CAMERA_MOVE;
        }
        if (keys[SDL_SCANCODE_S]) {
            forward = -SPEED_CAMERA_MOVE;
        }
        if (keys[SDL_SCANCODE_D]) {
            right = SPEED_CAMERA_MOVE;
        }
        if (keys[SDL_SCANCODE_SPACE]) {
            up = SPEED_CAMERA_MOVE;
        }
        if (keys[SDL_SCANCODE_LCTRL]) {
            up = -SPEED_CAMERA_MOVE;
        }
        camera_move(&s->camera, forward, right, up);
    }

    static mat4 model_matrix_car_player = GLM_MAT4_IDENTITY_INIT;
    // car controls
    vec3 axis = {0.0, 1.0, 0.0};
    if (keys[SDL_SCANCODE_LEFT]) {
        glm_rotate(model_matrix_car_player, SPEED_CAR_ROTATE, axis);
    }
    if (keys[SDL_SCANCODE_RIGHT]) {
        glm_rotate(model_matrix_car_player, -SPEED_CAR_ROTATE, axis);
    }
    if (keys[SDL_SCANCODE_UP]) {
        glm_translate(model_matrix_car_player, (vec3){0.0, 0.0, SPEED_CAR_MOVE});
    }
    if (keys[SDL_SCANCODE_DOWN]) {
        glm_translate(model_matrix_car_player, (vec3){0.0, 0.0, -SPEED_CAR_MOVE});
    }

    Uniform_Frame uniform_frame = {0};
    mat4 view_projection_matrix;

    if (third_person) {
        // set camera to car world space plus offset
        vec3 camera_offset = {0.0f, 7.0f, -12.5f};
        glm_mat4_mulv3(model_matrix_car_player, camera_offset, 1.0f, s->camera.pos);
        vec3 camera_lookat = {0.0, 0.0, 8.0};
        glm_mat4_mulv3(model_matrix_car_player, camera_lookat, 1.0f, s->camera.target);
    }
    camera_get_view_projection(&s->camera, o, view_projection_matrix);

    glm_mat4_copy(view_projection_matrix, uniform_frame.view_projection);

    wgpuQueueWriteBuffer(s->queue, s->ubo_model, 0, &model_matrix_car_player, sizeof(mat4));

    // self driving car

    static mat4 model_matrix_car_computer = {
        {1.0f, 0.0f, 0.0f, 0.0f},
        {0.0f, 1.0f, 0.0f, 0.0f},
        {0.0f, 0.0f, 1.0f, 0.0f},
        {10.0f, 0.0f, 0.0f, 1.0f}
    };

    glm_rotate(model_matrix_car_computer, SPEED_CAR_ROTATE, axis);
    glm_translate(model_matrix_car_computer, (vec3){0.0, 0.0, SPEED_CAR_MOVE});

    wgpuQueueWriteBuffer(s->queue, s->ubo_model, UBO_MODEL_SLOT_SIZE, &model_matrix_car_computer, sizeof(mat4));

    uint64_t freq = SDL_GetPerformanceFrequency();
    uniform_frame.time = (float)(SDL_GetPerformanceCounter() / (float)freq);
    wgpuQueueWriteBuffer(s->queue, s->ubo_frame, 0, &uniform_frame, sizeof(Uniform_Frame));
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
    wgpuRenderPassEncoderSetBindGroup(render_pass, 0, s->bg_frame, 1, &offset);
    model_render(&s->model_car, render_pass);

    offset = UBO_MODEL_SLOT_SIZE;
    wgpuRenderPassEncoderSetBindGroup(render_pass, 0, s->bg_frame, 1, &offset);
    model_render(&s->model_car, render_pass);

    offset = 2*UBO_MODEL_SLOT_SIZE;
    wgpuRenderPassEncoderSetBindGroup(render_pass, 0, s->bg_frame, 1, &offset);
    model_render(&s->model_city, render_pass);

    offset = 3*UBO_MODEL_SLOT_SIZE;
    wgpuRenderPassEncoderSetBindGroup(render_pass, 0, s->bg_frame, 1, &offset);
    model_render(&s->model_ground, render_pass);

    ImGui_ImplWGPU_RenderDrawData(ImGui::GetDrawData(), render_pass);

    // end render pass
    wgpuRenderPassEncoderEnd(render_pass);
    wgpuRenderPassEncoderRelease(render_pass);

    WGPUCommandBufferDescriptor command_buffer_desc = {0};
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
    texture_manager_terminate(s->tm);

    ImGui_ImplWGPU_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();

    SDL_Metal_DestroyView(s->metal_view);
    SDL_DestroyWindow(s->window);
    SDL_Quit();

    wgpuSurfaceRelease(s->surface);
    wgpuBindGroupRelease(s->bg_frame);
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
        .fov = 90,
        .window_width =  1000,
        .window_height = 800,
        .near_plane = 0.01,
        .far_plane = 100.0,
    };

    initialize(&s);

    bool running = true;
    while (running) {
        _update(&s, &o, &running);

        imgui_render(&o);

        _render(&s);
    }

    _terminate(&s);
}
