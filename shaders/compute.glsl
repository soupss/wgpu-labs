#version 450

layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;

layout(set = 0, binding = 0) uniform sampler u_sampler;
layout(set = 0, binding = 1) uniform texture2D u_src;
layout(set = 0, binding = 2, rgba8) uniform writeonly image2D u_dst;
layout(set = 0, binding = 3) uniform SrcDim {
    int w;
    int h;
} u_src_dim;


void main() {
    ivec2 dst_pos = ivec2(gl_GlobalInvocationID.xy);

    ivec2 src_pos1 = 2 * dst_pos;
    ivec2 src_pos2 = ivec2(src_pos1.x + 1, src_pos1.y);
    ivec2 src_pos3 = ivec2(src_pos1.x, src_pos1.y + 1);
    ivec2 src_pos4 = ivec2(src_pos1.x + 1, src_pos1.y + 1);

    vec2 src_dim = vec2(u_src_dim.w, u_src_dim.h);

    vec2 uv1 = (vec2(src_pos1) + 0.5) / src_dim;
    vec2 uv2 = (vec2(src_pos2) + 0.5) / src_dim;
    vec2 uv3 = (vec2(src_pos3) + 0.5) / src_dim;
    vec2 uv4 = (vec2(src_pos4) + 0.5) / src_dim;

    vec4 c1 = texture(sampler2D(u_src, u_sampler), uv1);
    vec4 c2 = texture(sampler2D(u_src, u_sampler), uv2);
    vec4 c3 = texture(sampler2D(u_src, u_sampler), uv3);
    vec4 c4 = texture(sampler2D(u_src, u_sampler), uv4);

    vec4 avg = (c1 + c2 + c3 + c4) / 4.0;

    imageStore(u_dst, dst_pos, c1);
}
