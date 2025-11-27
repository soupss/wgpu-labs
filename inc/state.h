#ifndef STATE_H
#define STATE_H

#include <SDL3/SDL.h>
#include <webgpu.h>
#include <cglm/cglm.h>
#include "model.h"
#include "camera.h"

//TODO: camera type
typedef struct {
    SDL_Window *window;
    SDL_MetalView metal_view;
    WGPUAdapter adapter;
    WGPUDevice device;
    WGPUInstance instance;
    WGPUSurface surface;
    WGPURenderPipeline pipeline;
    Camera camera;
    WGPUQueue queue;
    WGPUBindGroup bg;
    WGPUBuffer ubo_object;
    WGPUBuffer ubo_frame;
    Model model_car;
    Model model_city;
} State;

//TODO:camera ubo
typedef struct {
    mat4 view_projection;
    float time;
} UBOData_Frame;

typedef struct {
    mat4 model;
} UBOData_Object;

#endif
