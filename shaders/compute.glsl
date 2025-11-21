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

    // sample source mipmap
    vec2 src_pos[4];
    src_pos[0] = 2 * dst_pos;
    src_pos[1] = ivec2(src_pos[0].x + 1, src_pos[0].y);
    src_pos[2] = ivec2(src_pos[0].x, src_pos[0].y + 1);
    src_pos[3] = ivec2(src_pos[0].x + 1, src_pos[0].y + 1);

    ivec2 src_dim = ivec2(u_src_dim.w, u_src_dim.h);

    // average samples
    vec2 uv[4];
    vec4 c_tot = vec4(0.0);
    for (int i = 0; i < 4; i++) {
        uv[i] = src_pos[i] / src_dim;
        c_tot += texture(sampler2D(u_src, u_sampler), uv[i]);
    }

    vec4 avg = c_tot / 4.0;

    imageStore(u_dst, dst_pos, avg);
}
