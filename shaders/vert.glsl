#version 450

layout(set = 0, binding = 0) uniform Uniforms {
    float u_time;
};

layout(location = 0) in vec3 a_pos;
layout(location = 1) in vec3 a_color;

layout(location = 0) out vec3 v_color;

void main()
{
    float t = u_time;
    float warpx = 0.8 + 0.2*sin(3*t);
    float warpy = 1 + 0.1*sin(6*t);
    vec3 pos = vec3(a_pos.x * warpx, a_pos.y * warpy, a_pos.z);
    gl_Position = vec4(pos, 1.0);
    v_color = a_color;
}
