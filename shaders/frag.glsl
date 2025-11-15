#version 450

layout(set = 0, binding = 0) uniform Uniforms {
    float u_time;
};

layout(location = 0) in vec3 a_color;

layout(location = 0) out vec4 color;

void main()
{
    float t = u_time;
    vec2 res = vec2(1200, 800);
    vec2 uv = gl_FragCoord.xy / res;
    uv = uv * 2.0 - 1.0;
    float pulse = 0.5 + 0.5*sin(3*t);

    color = vec4(pulse * uv, 1.0, 1.0);
}
