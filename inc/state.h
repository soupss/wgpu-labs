#ifndef STATE_H
#define STATE_H

#include <SDL3/SDL.h>
#include <webgpu.h>
#include <cglm/cglm.h>
#include "constants.h"

typedef struct State {
    SDL_Window *window;
    SDL_MetalView metal_view;
    WGPUAdapter adapter;
    WGPUDevice device;
    WGPUInstance instance;
    WGPUSurface surface;
    WGPURenderPipeline pipeline;
    WGPUQueue queue;
    WGPUBuffer uniform_buffer;
    WGPUBuffer vertex_buffer;
    WGPUBuffer index_buffer;
    size_t vertex_buffer_size;
    WGPUTexture texture_asphalt;
    WGPUTexture texture_explosion;
    WGPUBindGroupLayout bgl;
    WGPUBindGroupEntry bg_asphalt_entries[BG_ENTRY_COUNT];
    WGPUBindGroup bg_asphalt;
    WGPUBindGroup bg_explosion;
} State;

typedef struct Uniforms {
    float time;
    mat4 projection_matrix;
    vec3 camera_position;
} Uniforms;

#endif
