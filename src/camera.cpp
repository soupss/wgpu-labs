#include "camera.h"
#include "constants.h"

void camera_get_view_projection(Camera *cam, mat4 out) {
    mat4 view = GLM_MAT4_IDENTITY_INIT;
    vec3 up = {0.0, 1.0, 0.0};
    glm_lookat(cam->pos,  cam->target, up, view);

    mat4 projection = GLM_MAT4_IDENTITY_INIT;
    float fovy = 45.0;
    float aspect_ratio = (float)WINDOW_WIDTH / (float)WINDOW_HEIGHT;
    float near_plane = 0.01f;
    float far_plane = 300.0f;
    glm_perspective(fovy, aspect_ratio, near_plane, far_plane, projection);

    glm_mat4_mul(projection, view, out);
}

static void _camera_update_basis(Camera* cam, vec3 f, vec3 r) {
    vec3 forward = {
        cosf(cam->pitch)*cosf(cam->yaw),
        sinf(cam->pitch),
        cosf(cam->pitch)*sinf(cam->yaw)
    };
    glm_vec3_normalize_to(forward, f);
    vec3 up = {0,1,0};
    glm_vec3_crossn(f, up, r);
}

void camera_move(Camera *cam, float fwd_amt, float right_amt, float up_amt) {
    vec3 f,r;
    _camera_update_basis(cam, f,r);
    vec3 delta = GLM_VEC3_ZERO_INIT, t;

    glm_vec3_scale(f, fwd_amt, t);
    glm_vec3_add(delta, t, delta);

    glm_vec3_scale(r, right_amt, t);
    glm_vec3_add(delta, t, delta);

    vec3 up = {0,1,0};
    glm_vec3_scale(up, up_amt, t);
    glm_vec3_add(delta, t, delta);

    glm_vec3_add(cam->pos, delta, cam->pos);
    vec3 target;
    glm_vec3_add(cam->pos, f, target);
    glm_vec3_copy(target, cam->target);
}
