#ifndef MODEL_H
#define MODEL_H

#include <stdlib.h>
#include <webgpu.h>
#include <cglm/cglm.h>
#include "texture_manager.h"
struct _State;
typedef struct _State State;

typedef struct {
    WGPUBindGroup bg;
    char *name;
} Material;

typedef struct {
    unsigned int i_start;
    unsigned int i_count;
    unsigned int i_material;
} Mesh;

typedef struct {
    Mesh *meshes;
    unsigned int mesh_count;

    Material *materials;
    unsigned int material_count;

    WGPUBuffer vbo;
} Model;

void model_load(State *s, const WGPUBindGroupLayout bgl_model, Model *model, const char *path_obj, const char *path_textures);

void model_render(Model *model, WGPURenderPassEncoder pass);

#endif
