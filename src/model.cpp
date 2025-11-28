#include "model.h"
#include <webgpu.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#define TINYOBJ_LOADER_C_IMPLEMENTATION
#include "tinyobj_loader_c.h"
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#include "util.h"
#include "constants.h"

typedef struct {
    float v[3];
    float n[3];
    float uv[2];
} Vertex;

static void _on_file_read(void *ctx,
                    const char *filename,
                    int is_mtl,
                    const char *obj_filename,
                    char **buf,
                    size_t *len)
{
    FILE *f = fopen(filename, "rb");
    if (!f) {
        fprintf(stderr, "tinyobj file_reader: could not open %s\n", filename);
        return;
    }
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (size <= 0) {
        fclose(f);
        return;
    }
    char *data = (char *)malloc(size);
    if (!data) {
        fclose(f);
        return;
    }
    size_t read_bytes = fread(data, 1, size, f);
    fclose(f);
    if (read_bytes != (size_t)size) {
        free(data);
        return;
    }
    *buf = data;
    *len = size;
}

static void _texturemap_fallback(const WGPUDevice device, const WGPUQueue queue, TextureMap *tm) {
    WGPUTextureDescriptor tex_desc = {
        .label = {"texturemap fallback",  WGPU_STRLEN},
        .size = {1, 1, 1},
        .mipLevelCount = 1,
        .sampleCount = 1,
        .dimension = WGPUTextureDimension_2D,
        .format = WGPUTextureFormat_RGBA8UnormSrgb,
        .usage = WGPUTextureUsage_TextureBinding | WGPUTextureUsage_CopyDst,
    };

    tm->texture = wgpuDeviceCreateTexture(device, &tex_desc);

    WGPUTexelCopyTextureInfo dest = {
        .texture = tm->texture,
        .mipLevel = 0,
        .origin = {0, 0, 0},
        .aspect = WGPUTextureAspect_All
    };

    WGPUTexelCopyBufferLayout data_layout = {
        .offset = 0,
        .bytesPerRow = 4* sizeof(char),
        .rowsPerImage = 1
    };

    WGPUExtent3D write_size = {
        .width = 1,
        .height = 1,
        .depthOrArrayLayers = 1
    };

    unsigned char pixel[4] = {255, 255, 255, 255};

    wgpuQueueWriteTexture(queue, &dest, pixel, 4 * sizeof(char), &data_layout, &write_size);

    tm->view = wgpuTextureCreateView(tm->texture, NULL);

    WGPUSamplerDescriptor sampler_desc = {
        .addressModeU = WGPUAddressMode_Repeat,
        .addressModeV = WGPUAddressMode_Repeat,
        .addressModeW = WGPUAddressMode_Repeat,
        .magFilter = WGPUFilterMode_Linear,
        .minFilter = WGPUFilterMode_Linear,
        .mipmapFilter = WGPUMipmapFilterMode_Linear,
        .lodMinClamp = 0.0f,
        .lodMaxClamp = 1000.0f,
        .maxAnisotropy = 1
    };

    tm->sampler = wgpuDeviceCreateSampler(device, &sampler_desc);
}

static void _texturemap_load(const WGPUDevice device, const WGPUQueue queue, TextureMap *tm, const char *path) {
    int w,h;
    const int channels = 4;
    unsigned char *pixels = stbi_load(path, &w, &h, NULL, channels);
    if (!pixels) {
        fprintf(stderr, "Failed to load image %s!\n", stbi_failure_reason());
        _texturemap_fallback(device, queue, tm);
        return;
    }

    WGPUTextureDescriptor tex_desc = {
        .label = {path,  WGPU_STRLEN},
        .size = {(uint32_t)w, (uint32_t)h, 1},
        .mipLevelCount = 1,
        .sampleCount = 1,
        .dimension = WGPUTextureDimension_2D,
        .format = WGPUTextureFormat_RGBA8UnormSrgb,
        .usage = WGPUTextureUsage_TextureBinding | WGPUTextureUsage_CopyDst,
    };

    tm->texture = wgpuDeviceCreateTexture(device, &tex_desc);

    WGPUTexelCopyTextureInfo dest = {
        .texture = tm->texture,
        .mipLevel = 0,
        .origin = {0, 0, 0},
        .aspect = WGPUTextureAspect_All
    };

    WGPUTexelCopyBufferLayout data_layout = {
        .offset = 0,
        .bytesPerRow = (uint32_t)(w * 4 * sizeof(unsigned char)),
        .rowsPerImage = (uint32_t)h
    };

    WGPUExtent3D write_size = {
        .width = (uint32_t)w,
        .height = (uint32_t)h,
        .depthOrArrayLayers = 1
    };

    wgpuQueueWriteTexture(queue, &dest, pixels, w * h * channels * sizeof(unsigned char), &data_layout, &write_size);

    stbi_image_free(pixels);

    tm->view = wgpuTextureCreateView(tm->texture, NULL);

    WGPUSamplerDescriptor sampler_desc = {
        .addressModeU = WGPUAddressMode_Repeat,
        .addressModeV = WGPUAddressMode_Repeat,
        .addressModeW = WGPUAddressMode_Repeat,
        .magFilter = WGPUFilterMode_Linear,
        .minFilter = WGPUFilterMode_Linear,
        .mipmapFilter = WGPUMipmapFilterMode_Linear,
        .lodMinClamp = 0.0f,
        .lodMaxClamp = 1000.0f,
        .maxAnisotropy = 1
    };
    tm->sampler = wgpuDeviceCreateSampler(device, &sampler_desc);
}

void model_load(const WGPUDevice device, const WGPUQueue queue, WGPUBindGroupLayout bgl_model, Model *model, const char *path_obj, const char *path_textures) {
    ////////////////////
    // parse obj file //
    ////////////////////

    tinyobj_attrib_t attrib;
    tinyobj_shape_t *shapes = NULL;
    size_t shape_count = 0;
    tinyobj_material_t *materials = NULL;
    size_t material_count = 0;

    tinyobj_attrib_init(&attrib);

    int r = tinyobj_parse_obj(&attrib, &shapes, &shape_count, &materials, &material_count, path_obj, _on_file_read, NULL, TINYOBJ_FLAG_TRIANGULATE);

    if (r != TINYOBJ_SUCCESS) {
        printf("Error loading OBJ file\n");
    }

    ///////////////
    // materials //
    ///////////////

    model->materials = (Material*)malloc(material_count * sizeof(Material));
    model->material_count = material_count;
    for (int i = 0; i < material_count; i ++) {
        Material *dst = &model->materials[i];
        tinyobj_material_t *src = &materials[i];

        memcpy(dst->ambient, src->ambient, 3 * sizeof(float));
        memcpy(dst->diffuse, src->diffuse, 3 * sizeof(float));
        memcpy(dst->specular, src->specular, 3 * sizeof(float));
        memcpy(dst->emission, src->emission, 3 * sizeof(float));
        dst->shininess = src->shininess;
        dst->refraction = src->ior;
        dst->dissolve = src->dissolve;
        dst->illumination = src->illum;

        TextureMap *tm = &dst->diffuse_map;
        char path_diffusemap[1024];

        u_get_texture_path(path_diffusemap, sizeof(path_diffusemap), path_textures, src->diffuse_texname);
        _texturemap_load(device, queue, tm, path_diffusemap);

        WGPUBufferDescriptor buf_desc = {
            .size = sizeof(Material),
            .usage = WGPUBufferUsage_Uniform | WGPUBufferUsage_CopyDst
        };

        WGPUBuffer buf = wgpuDeviceCreateBuffer(device, &buf_desc);
        wgpuQueueWriteBuffer(queue, buf, 0, dst, sizeof(Material));

        WGPUBindGroupEntry bg_entries[BG_MODEL_ENTRY_COUNT] = {
            {
                .binding = 0,
                .buffer = buf,
                .offset = 0,
                .size = sizeof(Material)
            },
            {
                .binding = 1,
                .textureView = tm->view
            },
            {
                .binding = 2,
                .sampler = tm->sampler
            }
        };

        dst->name = src->name;
        WGPUBindGroupDescriptor bg_desc = {
            .label = src->name,
            .entryCount = BG_MODEL_ENTRY_COUNT,
            .entries = bg_entries,
            .layout = bgl_model
        };

        dst->bg = wgpuDeviceCreateBindGroup(device, &bg_desc);
    }

    ////////////////
    // create vbo //
    ////////////////

    const int face_count = attrib.num_face_num_verts;

    // prefix sum of face start indices
    int *prefix = (int*)malloc((face_count + 1) * sizeof(int));
    prefix[0] = 0;
    for (int i = 0; i < face_count; i++) {
        prefix[i+1] = prefix[i] + attrib.face_num_verts[i];
    }

    const int vertex_count = prefix[face_count];

    Vertex *vertices = (Vertex*)malloc(vertex_count * sizeof(Vertex));
    for (int f = 0; f < face_count; f++) {
        int vert_per_face = attrib.face_num_verts[f];
        int i_start = prefix[f];
        for (int k = 0; k < vert_per_face; k++) {
            tinyobj_vertex_index_t vi = attrib.faces[i_start + k];
            int i_v = vi.v_idx;
            int i_n = vi.vn_idx;
            int i_t = vi.vt_idx;
            Vertex *v = &vertices[i_start + k];
            memcpy(v->v, &attrib.vertices[3 * i_v], 3 * sizeof(float));
            memcpy(v->n, &attrib.normals[3 * i_n], 3 * sizeof(float));
            memcpy(v->uv, &attrib.texcoords[2 * i_t], 2 * sizeof(float));
        }
    }

    const int vbo_size = vertex_count * sizeof(Vertex);

    WGPUBufferDescriptor vbo_desc = {
        .size = (uint64_t)vbo_size,
        .usage = WGPUBufferUsage_Vertex | WGPUBufferUsage_CopyDst,
        .mappedAtCreation = false
    };
    model->vbo = wgpuDeviceCreateBuffer(device, &vbo_desc);

    wgpuQueueWriteBuffer(queue, model->vbo, 0, vertices, vbo_size);

    ///////////////////
    // create meshes //
    ///////////////////

    model->mesh_count = 0;
    if (attrib.num_face_num_verts > 0) {
        model->mesh_count = 1;
        int last_material_id = attrib.material_ids[0];
        for (unsigned int i = 1; i < attrib.num_face_num_verts; i++) {
            if (attrib.material_ids[i] != last_material_id) {
                model->mesh_count++;
                last_material_id = attrib.material_ids[i];
            }
        }
    }

    model->meshes = (Mesh*)malloc(model->mesh_count * sizeof(Mesh));

    int current_mesh = 0;
    int last_material_id = attrib.material_ids[0];
    unsigned int mesh_start_face = 0;

    for (unsigned int i = 1; i < face_count; i++) {
        if (attrib.material_ids[i] != last_material_id) {
            Mesh *mesh = &model->meshes[current_mesh];
            mesh->i_material = last_material_id;
            mesh->i_start = prefix[mesh_start_face];
            mesh->i_count = prefix[i] - prefix[mesh_start_face];
            current_mesh++;
            last_material_id = attrib.material_ids[i];
            mesh_start_face = i;
        }

        Mesh *mesh = &model->meshes[current_mesh];
        mesh->i_material = last_material_id;
        mesh->i_start = prefix[mesh_start_face];
        mesh->i_count = prefix[attrib.num_face_num_verts] - prefix[mesh_start_face];
    }

    free(prefix);
}

void model_render(Model *model, WGPURenderPassEncoder pass) {
    wgpuRenderPassEncoderSetVertexBuffer(pass, 0, model->vbo, 0, WGPU_WHOLE_SIZE);
    for (int i = 0; i < model->mesh_count; i++) {
        Mesh mesh = model->meshes[i];
        int i_mat = mesh.i_material;
        Material mat = model->materials[i_mat];
        wgpuRenderPassEncoderSetBindGroup(pass, 1, mat.bg, 0, 0);
        wgpuRenderPassEncoderDraw(pass, mesh.i_count, 1, mesh.i_start, 0);
    }
}
