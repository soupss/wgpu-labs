#ifndef CONSTANTS_H
#define CONSTANTS_H

#define PATH_SHADER_VERTEX "build/vertex.spv"
#define PATH_SHADER_FRAGMENT "build/fragment.spv"
#define PATH_SHADER_COMPUTE "build/compute.spv"
#define PATH_TEXTURE_ASPHALT "assets/textures/asphalt.jpg"
#define PATH_TEXTURE_EXPLOSION "assets/textures/explosion.png"
#define PATH_MODEL_CAR "assets/models/car.obj"
#define PATH_MODEL_CITY "assets/models/city.obj"

#define WINDOW_WIDTH 1200
#define WINDOW_HEIGHT 800

#define VBO_STRIDE 32 // pos + norm + uv = (3 + 3 + 2) * 4
#define VERTEX_ATTRIBUTE_COUNT 3

#define UBO_OBJECT_SLOT_SIZE 256
#define UBO_OBJECT_SLOT_COUNT 2
#define UBO_OBJECT_SIZE (UBO_OBJECT_SLOT_SIZE * UBO_OBJECT_SLOT_COUNT)

#define BG_ENTRY_COUNT 3
#define BG_COMP_ENTRY_COUNT 4

#endif
