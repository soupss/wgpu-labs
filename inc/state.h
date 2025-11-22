#ifndef STATE_H
#define STATE_H

#include <SDL3/SDL.h>
#include <webgpu.h>
#include <cglm/cglm.h>
#include "constants.h"
#include "model.h"

typedef struct State {
    SDL_Window *window;
    SDL_MetalView metal_view;
    WGPUAdapter adapter;
    WGPUDevice device;
    WGPUInstance instance;
    WGPUSurface surface;
    WGPURenderPipeline pipeline;
    WGPUQueue queue;
    WGPUBindGroup bg;
    WGPUBuffer ubo_object;
    WGPUBuffer ubo_frame;
    // TODO: buf
    WGPUBuffer vbo_car;
    WGPUBuffer vbo_city;
    WGPUBuffer ibo_car;
    WGPUBuffer ibo_city;
    Mesh mesh_car;
    Mesh mesh_city;
} State;

typedef struct UBOData_Frame {
    mat4 view_projection;
    float time;
} UBOData_Frame;

typedef struct UBOData_Object {
    mat4 model;
} UBOData_Object;

#endif
