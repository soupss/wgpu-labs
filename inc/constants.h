#ifndef CONSTANTS_H
#define CONSTANTS_H

#define PATH_SHADER_VERTEX "build/vertex.spv"
#define PATH_SHADER_FRAGMENT "build/fragment.spv"
#define PATH_SHADER_COMPUTE "build/compute.spv"
#define PATH_TEXTURE_ASPHALT "assets/textures/asphalt.jpg"
#define PATH_TEXTURE_EXPLOSION "assets/textures/explosion.png"
#define PATH_MODEL_CAR "assets/models/car.obj"

#define WINDOW_WIDTH 1200
#define WINDOW_HEIGHT 800

#define VERTEX_BUFFER_STRIDE 32 // pos + norm + uv = (3 + 3 + 2) * 4
#define VERTEX_ATTRIBUTE_COUNT 3

#define BG_ENTRY_COUNT 3
#define BG_COMP_ENTRY_COUNT 4

#endif
