#version 450

layout(set = 0, binding = 0) uniform frame {
    mat4 u_view_projection;
    float u_time;
};

layout(set = 0, binding = 2) uniform sampler u_sampler;

layout(location = 0) in vec2 v_uv;

layout(location = 0) out vec4 color;

void main()
{
    float t = u_time;

    // color = texture(sampler2D(u_texture, u_sampler), v_uv);

    float d = length(v_uv);
    float circle = step(0.9, sin(d * 40 - t * 12.0));
    color = vec4(vec3(circle), 1.0);
}
