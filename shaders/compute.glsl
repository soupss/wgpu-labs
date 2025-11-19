#version 450

layout(local_size_x = 10, local_size_y = 1, local_size_z = 1) in;

layout(set = 0, binding = 0) buffer A {
    float data[];
} a;

void main() {
    uint i = gl_GlobalInvocationID.x;
    a.data[i] = a.data[i] * a.data[i];
}
