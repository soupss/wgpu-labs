#/bin/bash
rm -rf build &&
mkdir build &&
cmake . -B build &&
glslc -fshader-stage=vert shaders/vert.glsl -o build/vert.spv &&
glslc -fshader-stage=frag shaders/frag.glsl -o build/frag.spv &&
cmake --build build &&
./build/bin
