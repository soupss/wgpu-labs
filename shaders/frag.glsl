#version 450

layout(location = 0) in vec3 v_color;

layout(location = 0) out vec4 color;

layout(binding = 0) uniform UniformBuffer {
    vec4 u_color;
};

void main()
{
    color = vec4(v_color*u_color.xyz, 1.0);
}
