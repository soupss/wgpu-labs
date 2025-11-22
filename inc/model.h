#ifndef MODEL_H
#define MODEL_H

#include <stdlib.h>

typedef struct Mesh {
    float *vertices;
    unsigned int *indices;
    size_t vertex_count;
    size_t index_count;
} Mesh;

int model_load(const char *obj_path, Mesh *out_mesh);

#endif
