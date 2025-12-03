#ifndef STATE_H
#define STATE_H

#include <SDL3/SDL.h>
#include <webgpu.h>
#include <cglm/cglm.h>
#include "model.h"
#include "camera.h"

typedef struct {
    SDL_Window *window;
    SDL_MetalView metal_view;
    WGPUAdapter adapter;
    WGPUDevice device;
    WGPUInstance instance;
    WGPUSurface surface;
    WGPURenderPipeline pipeline;
    WGPUQueue queue;
    WGPUBindGroup bg_frame;
    WGPUSampler sampler;

    Camera camera;
    WGPUBuffer ubo_object;
    WGPUBuffer ubo_frame;
    Model model_car;
    Model model_city;
} State;

#endif
