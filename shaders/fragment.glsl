#version 450

layout(set = 0, binding = 0) uniform Uniforms {
    float u_time;
    mat4 u_projection_matrix;
    vec3 u_camera_position;
};

layout(set = 0, binding = 1) uniform sampler u_sampler;
layout(set = 0, binding = 2) uniform texture2D u_texture;

layout(location = 0) in vec2 v_uv;

layout(location = 0) out vec4 color;

void main()
{
    float t = u_time;

    color = texture(sampler2D(u_texture, u_sampler), v_uv);

    // float d = length(v_uv);
    // float circle = step(0.9, sin(d * 40 - t * 12.0));
    // color = vec4(vec3(circle), 1.0);
}
