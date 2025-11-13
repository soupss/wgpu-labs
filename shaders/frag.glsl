#version 450

layout(set = 0, binding = 0) uniform Uniforms {
    vec3 u_tint;
};

layout(location = 0) in vec3 a_color;

layout(location = 0) out vec4 color;

void main()
{
    vec3 c = a_color * u_tint;
    color = vec4(c, 1.0);
}
