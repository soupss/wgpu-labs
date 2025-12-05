#ifndef STATE_H
#define STATE_H

#include <SDL3/SDL.h>
#include <webgpu.h>
#include <cglm/cglm.h>
#include "model.h"
#include "camera.h"

typedef struct _State {
    SDL_Window *window;
    SDL_MetalView metal_view;

    Camera camera;
    Model model_car;
    Model model_city;
    Model model_ground;
    TextureManager *tm;

    WGPUAdapter adapter;
    WGPUDevice device;
    WGPUInstance instance;
    WGPUSurface surface;
    WGPURenderPipeline pipeline;
    WGPUQueue queue;
    WGPUBindGroup bg_frame;
    WGPUSampler sampler;
    WGPUShaderModule shadermodule_comp;
    WGPUBuffer ubo_model;
    WGPUBuffer ubo_frame;
} State;

#endif
