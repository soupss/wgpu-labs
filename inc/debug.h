#ifndef IMGUI_H
#define IMGUI_H

typedef struct {
    float fov;
    int window_width;
    int window_height;
    float near_plane;
    float far_plane;
} Options;

void imgui_render(Options *o);

#endif
