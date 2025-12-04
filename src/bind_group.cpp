#include "bind_group.h"
#include <webgpu.h>
#include "state.h"
#include "constants.h"
#include "util.h"

void bgls_create(const State *s, WGPUBindGroupLayout *bgls) {
    WGPUBindGroupLayoutEntry bgl_frame_entries[BG_FRAME_ENTRY_COUNT] = {
        {
            .binding = 0,
            .visibility = WGPUShaderStage_Fragment | WGPUShaderStage_Vertex,
            .buffer.type = WGPUBufferBindingType_Uniform,
        },
        {
            .binding = 1,
            .visibility = WGPUShaderStage_Vertex,
            .buffer.type = WGPUBufferBindingType_Uniform,
            .buffer.hasDynamicOffset = true,
            .buffer.minBindingSize = UBO_MODEL_SLOT_SIZE
        },
        {
            .binding = 2,
            .visibility = WGPUShaderStage_Fragment,
            .sampler.type = WGPUSamplerBindingType_Filtering
        },
    };

    WGPUBindGroupLayoutDescriptor bgl_frame_desc = {
        .label = {
            .data = "bgl",
            .length = WGPU_STRLEN
        },
        .nextInChain = NULL,
        .entryCount = BG_FRAME_ENTRY_COUNT,
        .entries = bgl_frame_entries
    };

    bgls[0] = wgpuDeviceCreateBindGroupLayout(s->device, &bgl_frame_desc);

    WGPUBindGroupLayoutEntry bgl_model_entries[BG_MODEL_ENTRY_COUNT] = {
        {
            .binding = 0,
            .visibility = WGPUShaderStage_Fragment,
            .buffer.type = WGPUBufferBindingType_Uniform,
        },
        {
            .binding = 1,
            .visibility = WGPUShaderStage_Fragment,
            .texture = {
                .sampleType = WGPUTextureSampleType_Float,
                .viewDimension = WGPUTextureViewDimension_2D,
                .multisampled = false,

            },
        },
        {
            .binding = 2,
            .visibility = WGPUShaderStage_Fragment,
            .texture = {
                .sampleType = WGPUTextureSampleType_Float,
                .viewDimension = WGPUTextureViewDimension_2D,
                .multisampled = false,

            },
        },

    };

    WGPUBindGroupLayoutDescriptor bgl_model_desc = {
        .label = {
            .data = "bgl_model",
            .length = WGPU_STRLEN
        },
        .nextInChain = NULL,
        .entryCount = BG_MODEL_ENTRY_COUNT,
        .entries = bgl_model_entries
    };

    bgls[1] = wgpuDeviceCreateBindGroupLayout(s->device, &bgl_model_desc);
}

void bg_create_frame(State *s, WGPUBindGroupLayout bgl) {
    WGPUBufferDescriptor ubo_frame_desc = {
        .nextInChain = NULL,
        .usage = WGPUBufferUsage_Uniform | WGPUBufferUsage_CopyDst,
        .size = sizeof(Uniform_Frame),
        .mappedAtCreation = false
    };
    s->ubo_frame = wgpuDeviceCreateBuffer(s->device, &ubo_frame_desc);

    WGPUBufferDescriptor ubo_model_desc = {
        .nextInChain = NULL,
        .usage = WGPUBufferUsage_Uniform | WGPUBufferUsage_CopyDst,
        .size = UBO_MODEL_SIZE,
        .mappedAtCreation = false
    };
    s->ubo_model = wgpuDeviceCreateBuffer(s->device, &ubo_model_desc);

    Uniform_ModelMatrix u_model_matrix_car = {
        .model = GLM_MAT4_IDENTITY_INIT
    };
    glm_translate(u_model_matrix_car.model, (vec3){0.0, 5.0, 0.0});

    Uniform_ModelMatrix u_model_matrix_city = {
        .model = GLM_MAT4_IDENTITY_INIT
    };
    glm_translate(u_model_matrix_city.model, (vec3){0.0, 0.0, 0.0});

    wgpuQueueWriteBuffer(s->queue, s->ubo_model, 0, &u_model_matrix_car, sizeof(Uniform_ModelMatrix));
    wgpuQueueWriteBuffer(s->queue, s->ubo_model, UBO_MODEL_SLOT_SIZE, &u_model_matrix_city, sizeof(Uniform_ModelMatrix));

    WGPUBindGroupEntry bg_entries[BG_FRAME_ENTRY_COUNT] = {
        {
            .binding = 0,
            .buffer = s->ubo_frame,
            .offset = 0,
            .size = sizeof(Uniform_Frame)
        },
        {
            .binding = 1,
            .buffer = s->ubo_model,
            .offset = 0,
            .size = UBO_MODEL_SLOT_SIZE
        },
        {
            .binding = 2,
            .sampler = s->tm->sampler
        }
    };

    WGPUBindGroupDescriptor bg_desc = {
        .nextInChain = NULL,
        .layout = bgl,
        .entryCount = BG_FRAME_ENTRY_COUNT,
        .entries = bg_entries
    };
    s->bg_frame = wgpuDeviceCreateBindGroup(s->device, &bg_desc);
}

void bg_create_material(State *s, WGPUBindGroupLayout bgl, Material *dst, tinyobj_material_t *src, const char *path_textures) {
    // binding 0 (material)
    Uniform_Material uniform_material = {0};

    memcpy(uniform_material.ambient, src->ambient, 3 * sizeof(float));
    uniform_material.ambient[3] = 1.0;
    memcpy(uniform_material.diffuse, src->diffuse, 3 * sizeof(float));
    uniform_material.diffuse[3] = 1.0;
    memcpy(uniform_material.specular, src->specular, 3 * sizeof(float));
    uniform_material.specular[3] = 1.0;
    memcpy(uniform_material.emission, src->emission, 3 * sizeof(float));
    uniform_material.emission[3] = 1.0;
    uniform_material.shininess = src->shininess;
    uniform_material.refraction = src->ior;
    uniform_material.dissolve = src->dissolve;
    uniform_material.illumination = src->illum;

    WGPUBufferDescriptor buf_desc = {
        .size = sizeof(Uniform_Material),
        .usage = WGPUBufferUsage_Uniform | WGPUBufferUsage_CopyDst
    };

    WGPUBuffer buf = wgpuDeviceCreateBuffer(s->device, &buf_desc);
    wgpuQueueWriteBuffer(s->queue, buf, 0, &uniform_material, sizeof(Uniform_Material));

    // binding 1&2 (texture maps)
    WGPUTextureView view_diffuse_map = {0};
    if (src->diffuse_texname == NULL) {
        view_diffuse_map = texture_manager_get_view_identity(s->tm);
    }
    else {
        char path_diffuse_map[1024];
        u_get_texture_path(path_diffuse_map, sizeof(path_diffuse_map), path_textures, src->diffuse_texname);
        view_diffuse_map = texture_manager_get_view(s->tm, path_diffuse_map);
    }

    WGPUTextureView view_emission_map = {0};
    if (src->emission_texname == NULL) {
        view_emission_map = texture_manager_get_view_identity(s->tm);
    }
    else {
        char path_emission_map[1024];
        u_get_texture_path(path_emission_map, sizeof(path_emission_map), path_textures, src->emission_texname);
        view_emission_map = texture_manager_get_view(s->tm, path_emission_map);
    }

    WGPUBindGroupEntry bg_entries[BG_MODEL_ENTRY_COUNT] = {
        {
            .binding = 0,
            .buffer = buf,
            .offset = 0,
            .size = sizeof(Uniform_Material)
        },
        {
            .binding = 1,
            .textureView = view_diffuse_map
        },
        {
            .binding = 2,
            .textureView = view_emission_map
        },
    };

    WGPUBindGroupDescriptor bg_desc = {
        .label = src->name,
        .entryCount = BG_MODEL_ENTRY_COUNT,
        .entries = bg_entries,
        .layout = bgl
    };

    dst->bg = wgpuDeviceCreateBindGroup(s->device, &bg_desc);
}
