#ifndef MODEL_H
#define MODEL_H

#include <stdlib.h>
#include <webgpu.h>
#include <cglm/cglm.h>

typedef struct {
    WGPUTexture texture;
    WGPUTextureView view;
    WGPUSampler sampler;
} TextureMap;

typedef struct {
    const char *name;
    float ambient[4];
    float diffuse[4];
    float specular[4];
    float emission[4];
    float shininess;
    float refraction;
    float dissolve;
    unsigned int illumination;
    WGPUBindGroup bg;

    TextureMap diffuse_map;
    TextureMap emission_map;
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

void model_load(const WGPUDevice device, const WGPUQueue queue, WGPUBindGroupLayout bgl_model, Model *model, const char *path_obj, const char *path_textures);

void model_render(Model *model, WGPURenderPassEncoder pass);

#endif
