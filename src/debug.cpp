#include <imgui_impl_sdl3.h>
#include <imgui_impl_wgpu.h>
#include "debug.h"

void imgui_render(Options *o) {
    ImGui_ImplSDL3_NewFrame();
    ImGui_ImplWGPU_NewFrame();
    ImGui::NewFrame();
    ImGui::Begin("Options");
    ImGui::SliderFloat("Field of view", &o->fov, 30, 110);
    ImGui::SliderInt("Width", &o->window_width, 100, 1920);
    ImGui::SliderInt("Height", &o->window_height, 100, 1080);
    ImGui::SliderFloat("Near Plane", &o->near_plane, 0.001, 100.0);
    ImGui::SliderFloat("Far Plane", &o->far_plane, 10.0, 1000.0);
    ImGui::Text("Aspect Ratio: %f", (float)o->window_width / o->window_height);
    ImGui::End();

    ImGui::Render();
}
