#version 450

layout(set = 0, binding = 0) uniform frame {
    mat4 u_view_projection;
    float u_time;
};

layout(set = 0, binding = 1) uniform object {
    mat4 u_model;
};

layout(location = 0) in vec3 a_pos;
layout(location = 1) in vec3 a_norm;
layout(location = 2) in vec2 a_uv;

layout(location = 0) out vec2 v_uv;

void main()
{
    float t = u_time;

    gl_Position = u_view_projection * u_model * vec4(a_pos, 1.0);

    v_uv = a_uv;
}
