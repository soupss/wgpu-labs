#define WINDOW_WIDTH 800
#define WINDOW_HEIGHT 600

#define PATH_VERTEX_SHADER "build/vert.spv"
#define PATH_FRAGMENT_SHADER "build/frag.spv"

#define WEBGPU_STR(str) (WGPUStringView){.data = str, .length = sizeof(str) - 1}
