#ifndef CONSTANTS_H
#define CONSTANTS_H

#define PATH_SHADER_VERTEX "build/vertex.spv"
#define PATH_SHADER_FRAGMENT "build/fragment.spv"
#define PATH_SHADER_COMPUTE "build/compute.spv"
#define PATH_TEXTURE_ASPHALT "assets/textures/asphalt.jpg"
#define PATH_TEXTURE_EXPLOSION "assets/textures/explosion.png"

#define WINDOW_WIDTH 1200
#define WINDOW_HEIGHT 800

#define VERTEX_COUNT_QUAD 4
#define VERTEX_COUNT_TOTAL (2 * VERTEX_COUNT_QUAD)
#define VERTEX_BUFFER_STRIDE 5 // 3 pos + 2 uv
#define VERTEX_ATTRIBUTE_COUNT 2

#define INDEX_COUNT_QUAD 6
#define INDEX_COUNT_TOTAL (2 * INDEX_COUNT_QUAD)

#define BG_ENTRY_COUNT 3
#define BG_COMP_ENTRY_COUNT 4

#endif
