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

void _update_sampler(State *s, const Options *o) {
    WGPUSamplerDescriptor sampler_desc = {
        .addressModeU = WGPUAddressMode_ClampToEdge,
        .addressModeV = WGPUAddressMode_Repeat,
        .addressModeW = WGPUAddressMode_ClampToEdge,
        .magFilter = (WGPUFilterMode)o->mag_filter,
        .minFilter = (WGPUFilterMode)o->min_filter,
        .mipmapFilter = (WGPUMipmapFilterMode)o->mipmap_filter,
        .lodMinClamp = 0.0f,
        .lodMaxClamp = 1000.0f,
        .compare = WGPUCompareFunction_Undefined,
        .maxAnisotropy = (uint16_t)o->max_anisotropy
    };

    WGPUSampler sampler = wgpuDeviceCreateSampler(s->device, &sampler_desc);

    s->bg_asphalt_entries[1].sampler = sampler;

    WGPUBindGroupDescriptor bg_asphalt_desc = {
        .nextInChain = NULL,
        .layout = s->bgl,
        .entryCount = BG_ENTRY_COUNT,
        .entries = s->bg_asphalt_entries
    };

    s->bg_asphalt = wgpuDeviceCreateBindGroup(s->device, &bg_asphalt_desc);
}

void _render_imgui(Options *o) {
    ImGui_ImplSDL3_NewFrame();
    ImGui_ImplWGPU_NewFrame();
    ImGui::NewFrame();

	ImGui::PushID("magFilter");
	ImGui::Text("Magnification");
	ImGui::RadioButton("WGPUFilterMode_Nearest", &o->mag_filter, WGPUFilterMode_Nearest);
	ImGui::RadioButton("WGPUFilterMode_Linear", &o->mag_filter, WGPUFilterMode_Linear);
	ImGui::PopID();

	ImGui::PushID("minFilter");
	ImGui::Text("Minification");
	ImGui::RadioButton("WGPUFilterMode_Nearest", &o->min_filter, WGPUFilterMode_Nearest);
	ImGui::RadioButton("WGPUFilterMode_Linear", &o->min_filter, WGPUFilterMode_Linear);
	ImGui::PopID();

	ImGui::PushID("mipmapFilter");
	ImGui::Text("Minification");
	ImGui::RadioButton("WGPUMipmapFilterMode_Nearest", &o->mipmap_filter, WGPUMipmapFilterMode_Nearest);
	ImGui::RadioButton("WGPUMipmapFilterMode_Linear", &o->mipmap_filter, WGPUMipmapFilterMode_Linear);
	ImGui::PopID();

    bool all_linear =
    o->mag_filter == WGPUFilterMode_Linear &&
    o->min_filter == WGPUFilterMode_Linear &&
    o->mipmap_filter == WGPUMipmapFilterMode_Linear;

    ImGui::BeginDisabled(!all_linear);
    ImGui::SliderInt("maxAnisotropy", &o->max_anisotropy, 1, 16, "%");
    ImGui::EndDisabled();
    if (!all_linear) {
        o->max_anisotropy = 1;
    }

	ImGui::Dummy({ 0, 20 });
	ImGui::SliderFloat("Camera Panning", &o->camera_pan, -1.0, 1.0);
	ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);

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
    wgpuRenderPassEncoderSetIndexBuffer(render_pass, s->index_buffer, WGPUIndexFormat_Uint32, 0, INDEX_COUNT_TOTAL * sizeof(int));
    wgpuRenderPassEncoderSetBindGroup(render_pass, 0, s->bg_asphalt, 0, NULL);
    wgpuRenderPassEncoderDrawIndexed(render_pass, INDEX_COUNT_QUAD, 1, 0, 0, 0);

    wgpuRenderPassEncoderSetBindGroup(render_pass, 0, s->bg_explosion, 0, NULL);
    wgpuRenderPassEncoderDrawIndexed(render_pass, INDEX_COUNT_QUAD, 1, INDEX_COUNT_QUAD, 0, 0);

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
        .vertex_buffer_size = 0,
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

    const float vertices[VERTEX_COUNT_TOTAL * VERTEX_BUFFER_STRIDE] = {
        // asphalt
		-10.0f, 0.0f, -10.0f, 0.0f, 0.0f,
		-10.0f, 0.0f, -330.0f, 0.0f, 15.0f,
		10.0f,  0.0f, -330.0f, 1.0f, 15.0f,
		10.0f,  0.0f, -10.0f, 1.0f, 0.0f,
        // explosion
        2.0f, 0.0f, -22.0f, 0.0f, 0.0f,
        2.0f, 10.0f, -22.0f, 0.0f, 1.0f,
        12.0f, 10.0f, -22.0f, 1.0f, 1.0f,
        12.0f, 0.0f, -22.0f, 1.0f, 0.0f
    };
    s.vertex_buffer_size = sizeof(vertices);
    wgpuQueueWriteBuffer(s.queue, s.vertex_buffer, 0, vertices, sizeof(vertices));

    const int indices[INDEX_COUNT_TOTAL] = {
        // asphalt
        0, 1, 2,
        0, 2, 3,
        // explosion
        4, 5, 6,
        4, 6, 7
    };
    wgpuQueueWriteBuffer(s.queue, s.index_buffer, 0, indices, sizeof(indices));

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
        vec3 camera_position = {o.camera_pan, 10, 0};
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

        _update_sampler(&s, &o);

        _render(&s);
    }

    _terminate(&s);
}
