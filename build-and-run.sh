#/bin/bash
rm -rf build &&
mkdir build &&
cmake . -B build &&
glslc -fshader-stage=vertex shaders/vertex.glsl -o build/vertex.spv &&
glslc -fshader-stage=fragment shaders/fragment.glsl -o build/fragment.spv &&
glslc -fshader-stage=compute shaders/compute.glsl -o build/compute.spv &&
cmake --build build &&
./build/bin
