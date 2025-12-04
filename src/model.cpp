#include "model.h"
#include <webgpu.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include "bind_group.h"
#include "state.h"

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
                    size_t *len) {
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

void model_load(State *s, const WGPUBindGroupLayout bgl_model, Model *model, const char *path_obj, const char *path_textures) {
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
        printf("Error loading OBJ file: ");
        if (r == TINYOBJ_ERROR_EMPTY) {
            printf("TINYOBJ_ERROR_EMPTY\n");
        } else if (r == TINYOBJ_ERROR_INVALID_PARAMETER) {
            printf("TINYOBJ_ERROR_INVALID_PARAMETER\n");
        } else if (r == TINYOBJ_ERROR_FILE_OPERATION) {
            printf("TINYOBJ_ERROR_FILE_OPERATION\n");
        } else {
            printf("Unknown error\n");
        }
    } else {
        printf("Loaded '%s': %d vertices, %d normals, %d texcoords, %d faces\n",
               path_obj, attrib.num_vertices, attrib.num_normals, attrib.num_texcoords, attrib.num_face_num_verts);
    }

    ///////////////
    // materials //
    ///////////////

    model->materials = (Material*)malloc(material_count * sizeof(Material));
    model->material_count = material_count;
    for (int i = 0; i < material_count; i ++) {
        Material *dst = &(model->materials[i]);
        tinyobj_material_t *src = &(materials[i]);
        dst->name = src->name;
        bg_create_material(s, bgl_model, dst, src, path_textures);
    }

    ////////////////
    // create vbo //
    ////////////////

    //TODO: index buffer

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
            if(i_t == -1) printf("!!\n");
            Vertex *v = &vertices[i_start + k];
            memcpy(v->v, &attrib.vertices[3 * i_v], 3 * sizeof(float));
            memcpy(v->n, &attrib.normals[3 * i_n], 3 * sizeof(float));
            v->uv[0] = attrib.texcoords[2 * i_t + 0];
            v->uv[1] = attrib.texcoords[2 * i_t + 1];
        }
    }

    const int vbo_size = vertex_count * sizeof(Vertex);

    WGPUBufferDescriptor vbo_desc = {
        .size = (uint64_t)vbo_size,
        .usage = WGPUBufferUsage_Vertex | WGPUBufferUsage_CopyDst,
        .mappedAtCreation = false
    };
    model->vbo = wgpuDeviceCreateBuffer(s->device, &vbo_desc);

    wgpuQueueWriteBuffer(s->queue, model->vbo, 0, vertices, vbo_size);
    free(vertices);

    ///////////////////
    // create meshes //
    ///////////////////

    model->mesh_count = 0;
    if (face_count > 0) {
        model->mesh_count = 1;
        int last_material_id = attrib.material_ids[0];
        for (unsigned int i = 1; i < face_count; i++) {
            if (attrib.material_ids[i] != last_material_id) {
                model->mesh_count++;
                last_material_id = attrib.material_ids[i];
            }
        }
    }

    if (model->mesh_count > 0) {
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
        }

        // Final mesh
        Mesh *mesh = &model->meshes[current_mesh];
        mesh->i_material = last_material_id;
        mesh->i_start = prefix[mesh_start_face];
        mesh->i_count = prefix[face_count] - prefix[mesh_start_face];
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
