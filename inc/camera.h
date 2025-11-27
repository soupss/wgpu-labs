#ifndef CAMERA_H
#define CAMERA_H

#include "cglm/cglm.h"

typedef struct {
    vec3 pos;
    vec3 target;
    vec3 forward;
    vec3 right;
    vec3 up;
    float yaw;
    float distance;
    float pitch;
} Camera;

void camera_update(Camera *cam);

void camera_get_view_projection(Camera *cam, mat4 out);

void camera_rebuild_orbit(Camera* c);

void camera_move(Camera *cam, float f, float r, float u);

#endif
