#ifndef BIND_GROUP_H
#define BIND_GROUP_H

#include <cglm/cglm.h>
#include "state.h"

#include "tinyobj_loader_c.h"

typedef struct {
    mat4 view_projection;
    float time;
} Uniform_Frame;

typedef struct {
    mat4 model;
} Uniform_ModelMatrix;

typedef struct {
    float ambient[4];
    float diffuse[4];
    float specular[4];
    float emission[4];
    float shininess;
    float refraction;
    float dissolve;
    unsigned int illumination;
} Uniform_Material;

void bgls_create(const State *s, WGPUBindGroupLayout *bgls);

void bg_create_frame(State *s, WGPUBindGroupLayout bgl);

void bg_create_material(WGPUDevice device, WGPUQueue queue, WGPUBindGroupLayout bgl, Material *dst, tinyobj_material_t *src, const char *path_textures);

#endif
