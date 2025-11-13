#include <stdio.h>
#include <webgpu.h>
#include <SDL3/SDL.h>
#include <utility>

#include <iostream>
#include <cassert>
#include <vector>

#include "../constants.cpp"

WGPUBindGroup create_bind_group(WGPUDevice device, WGPUBindGroupLayout bind_group_layout, WGPUBuffer buffer, uint32_t binding_index, size_t buffer_size) {
    // Create a binding
    WGPUBindGroupEntry binding = {};
    // The index of the binding (the entries in bindGroupDesc can be in any order)
    binding.binding = binding_index;
    // The buffer it is actually bound to
    binding.buffer = buffer;
    // We can specify an offset within the buffer, so that a single buffer can hold
    // multiple uniform blocks.
    binding.offset = 0;
    // And we specify again the size of the buffer.
    binding.size = buffer_size;

    // A bind group contains one or multiple bindings
    WGPUBindGroupDescriptor bindGroupDesc = {};
    bindGroupDesc.layout = bind_group_layout;
    // There must be as many bindings as declared in the layout!
    bindGroupDesc.entryCount = 1;
    bindGroupDesc.entries = &binding;
    return wgpuDeviceCreateBindGroup(device, &bindGroupDesc);
}

WGPUBuffer create_buffer(WGPUDevice device, size_t buffer_size, WGPUBufferUsage usage) {
    WGPUBufferDescriptor buffer_desc = {};
    buffer_desc.nextInChain = NULL;
    buffer_desc.usage = usage;
    buffer_desc.size = buffer_size;
    buffer_desc.mappedAtCreation = false;
    return wgpuDeviceCreateBuffer(device, &buffer_desc);
}


WGPUVertexBufferLayout * get_vertex_buffer_layouts() {
    WGPUVertexAttribute vertex_attributes[2];
    vertex_attributes[0].format = WGPUVertexFormat_Float32x3;
    vertex_attributes[0].offset = 0;
    vertex_attributes[0].shaderLocation = 0;
    vertex_attributes[1].format = WGPUVertexFormat_Float32x3;
    vertex_attributes[1].offset = 3 * sizeof(float);
    vertex_attributes[1].shaderLocation = 1;
    
    WGPUVertexBufferLayout vertex_buffer_layout = {};
    vertex_buffer_layout.stepMode = WGPUVertexStepMode_Vertex;
    vertex_buffer_layout.arrayStride = 6 * sizeof(float);
    vertex_buffer_layout.attributeCount = 2;
    vertex_buffer_layout.attributes = vertex_attributes;

    WGPUVertexBufferLayout* out = (WGPUVertexBufferLayout*) malloc(sizeof(WGPUVertexBufferLayout));
    *out = vertex_buffer_layout;
    return out;

}