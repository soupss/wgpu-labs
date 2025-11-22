#include "model.h"
#include <stdio.h>
#include <string.h>
#define TINYOBJ_LOADER_C_IMPLEMENTATION
#include "tinyobj_loader_c.h"

static void _callback_read_file_all(void* ctx, const char* filename, int is_mtl,
                          const char* obj_filename, char** buf, size_t* len) {
    (void)ctx;
    (void)is_mtl;
    (void)obj_filename;
    *buf = NULL;
    *len = 0;
    FILE* f = fopen(filename, "rb");
    if (!f) return;
    fseek(f, 0, SEEK_END);
    long L = ftell(f);
    fseek(f, 0, SEEK_SET);
    char* data = (char*)malloc((size_t)L + 1);
    size_t r = fread(data, 1, (size_t)L, f);
    fclose(f);
    data[r] = '\0';
    *buf = data;
    *len = r + 1;  // include NUL
}

// chatgpt function
int model_load(const char *obj_path, Mesh *out_mesh) {
    if (!out_mesh) return -1;

    tinyobj_attrib_t attrib = {0};
    tinyobj_shape_t* shapes = NULL;
    size_t num_shapes = 0;
    tinyobj_material_t* materials = NULL;
    size_t num_materials = 0;

    int ret = tinyobj_parse_obj(
        &attrib, &shapes, &num_shapes,
        &materials, &num_materials,
        obj_path, _callback_read_file_all, NULL,
        TINYOBJ_FLAG_TRIANGULATE
    );

    if (ret != TINYOBJ_SUCCESS)
        return ret;

    size_t corner_count = attrib.num_faces;  // total number of vertex references
    float* verts = (float*)malloc(sizeof(float) * 8 * corner_count);
    unsigned int* inds = (unsigned int*)malloc(sizeof(unsigned int) * corner_count);

    if (!verts || !inds) {
        free(verts);
        free(inds);
        tinyobj_attrib_free(&attrib);
        tinyobj_shapes_free(shapes, num_shapes);
        tinyobj_materials_free(materials, num_materials);
        return -2;
    }

    size_t w = 0;
    for (size_t i = 0; i < corner_count; i++) {
        tinyobj_vertex_index_t vi = attrib.faces[i];

        float px = 0, py = 0, pz = 0;
        if (vi.v_idx >= 0 && vi.v_idx < (int)attrib.num_vertices) {
            px = attrib.vertices[3 * vi.v_idx + 0];
            py = attrib.vertices[3 * vi.v_idx + 1];
            pz = attrib.vertices[3 * vi.v_idx + 2];
        }

        float nx = 0, ny = 0, nz = 1;
        if (vi.vn_idx >= 0 && vi.vn_idx < (int)attrib.num_normals) {
            nx = attrib.normals[3 * vi.vn_idx + 0];
            ny = attrib.normals[3 * vi.vn_idx + 1];
            nz = attrib.normals[3 * vi.vn_idx + 2];
        }

        float u = 0, v = 0;
        if (vi.vt_idx >= 0 && vi.vt_idx < (int)attrib.num_texcoords) {
            u = attrib.texcoords[2 * vi.vt_idx + 0];
            v = attrib.texcoords[2 * vi.vt_idx + 1];
        }

        verts[w + 0] = px;
        verts[w + 1] = py;
        verts[w + 2] = pz;
        verts[w + 3] = nx;
        verts[w + 4] = ny;
        verts[w + 5] = nz;
        verts[w + 6] = u;
        verts[w + 7] = v;
        w += 8;

        inds[i] = (unsigned int)i;
    }

    // Fill mesh
    out_mesh->vertices = verts;
    out_mesh->indices = inds;
    out_mesh->vertex_count = corner_count;
    out_mesh->index_count = corner_count;

    // Cleanup tinyobj
    tinyobj_attrib_free(&attrib);
    tinyobj_shapes_free(shapes, num_shapes);
    tinyobj_materials_free(materials, num_materials);

    return 0;
}

