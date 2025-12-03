#include "bind_group.h"
#include <webgpu.h>
#include "state.h"
#include "constants.h"
#include "util.h"

//TODO: is state needed to pass?
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
            .buffer.minBindingSize = UBO_OBJECT_SLOT_SIZE
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
    //TODO: remove ubo from state
    s->ubo_frame = wgpuDeviceCreateBuffer(s->device, &ubo_frame_desc);

    WGPUBufferDescriptor ubo_object_desc = {
        .nextInChain = NULL,
        .usage = WGPUBufferUsage_Uniform | WGPUBufferUsage_CopyDst,
        .size = UBO_OBJECT_SIZE,
        .mappedAtCreation = false
    };
    s->ubo_object = wgpuDeviceCreateBuffer(s->device, &ubo_object_desc);

    Uniform_ModelMatrix u_model_matrix_car = {
        .model = GLM_MAT4_IDENTITY_INIT
    };
    glm_translate(u_model_matrix_car.model, (vec3){0.0, 5.0, 0.0});

    Uniform_ModelMatrix u_model_matrix_city = {
        .model = GLM_MAT4_IDENTITY_INIT
    };
    glm_translate(u_model_matrix_city.model, (vec3){0.0, 0.0, 0.0});

    wgpuQueueWriteBuffer(s->queue, s->ubo_object, 0, &u_model_matrix_car, sizeof(Uniform_ModelMatrix));
    wgpuQueueWriteBuffer(s->queue, s->ubo_object, UBO_OBJECT_SLOT_SIZE, &u_model_matrix_city, sizeof(Uniform_ModelMatrix));

    WGPUBindGroupEntry bg_entries[BG_FRAME_ENTRY_COUNT] = {
        {
            .binding = 0,
            .buffer = s->ubo_frame,
            .offset = 0,
            .size = sizeof(Uniform_Frame)
        },
        {
            .binding = 1,
            .buffer = s->ubo_object,
            .offset = 0,
            .size = UBO_OBJECT_SLOT_SIZE
        },
        {
            .binding = 2,
            .sampler = s->sampler
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

void bg_create_material(WGPUDevice device, WGPUQueue queue, WGPUBindGroupLayout bgl, Material *dst, tinyobj_material_t *src, const char *path_textures) {
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

    printf("mat: %s, emission: (%f, %f, %f)\n", dst->name, src->emission[0], src->emission[1], src->emission[2]);

    WGPUBufferDescriptor buf_desc = {
        .size = sizeof(Uniform_Material),
        .usage = WGPUBufferUsage_Uniform | WGPUBufferUsage_CopyDst
    };

    WGPUBuffer buf = wgpuDeviceCreateBuffer(device, &buf_desc);
    wgpuQueueWriteBuffer(queue, buf, 0, &uniform_material, sizeof(Uniform_Material));

    // binding 1&2 (texture maps)
    printf("diffuse map\n");
    TextureMap *tm_diffuse = &dst->diffuse_map;
    char path_diffusemap[1024];
    u_get_texture_path(path_diffusemap, sizeof(path_diffusemap), path_textures, src->diffuse_texname);
    printf("%s + %s became %s\n", path_textures,src->diffuse_texname, path_diffusemap);
    u_texturemap_load(device, queue, tm_diffuse, path_diffusemap);

    printf("emission map\n");
    TextureMap *tm_emission = &dst->emission_map;
    char path_emissionmap[1024];
    u_get_texture_path(path_emissionmap, sizeof(path_emissionmap), path_textures, src->emission_texname);
    printf("%s + %s became %s\n", path_textures,src->emission_texname, path_emissionmap);
    u_texturemap_load(device, queue, tm_emission, path_emissionmap);

    WGPUBindGroupEntry bg_entries[BG_MODEL_ENTRY_COUNT] = {
        {
            .binding = 0,
            .buffer = buf,
            .offset = 0,
            .size = sizeof(Uniform_Material)
        },
        {
            .binding = 1,
            .textureView = tm_diffuse->view
        },
        {
            .binding = 2,
            .textureView = tm_emission->view
        },
    };

    WGPUBindGroupDescriptor bg_desc = {
        .label = src->name,
        .entryCount = BG_MODEL_ENTRY_COUNT,
        .entries = bg_entries,
        .layout = bgl
    };

    dst->bg = wgpuDeviceCreateBindGroup(device, &bg_desc);
}
