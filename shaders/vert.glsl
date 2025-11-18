#version 450

layout(set = 0, binding = 0) uniform Uniforms {
    float u_time;
    mat4 u_projection_matrix;
    vec3 u_camera_position;
};

layout(location = 0) in vec3 a_pos;
layout(location = 1) in vec2 a_uv;

layout(location = 0) out vec2 v_uv;

void main()
{
    float t = u_time;

    mat3 rot = mat3(
            1.0, 0.0, 0.0,
            0.0, 0.97, 0.26,
            0.0, -0.26, 0.97
            );

    vec3 pos = vec3((rot * a_pos) - u_camera_position);

    gl_Position = u_projection_matrix * vec4(pos, 1.0);

    v_uv = a_uv;
}
