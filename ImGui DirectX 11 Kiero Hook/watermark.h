#pragma once
#include "imgui/imgui.h"





void RenderWatermark() {
    auto io = ImGui::GetIO();
    ImVec2 WindowSize, WindowPos;
    ImGui::SetNextWindowPos(ImVec2(15.0f, 30.0f), ImGuiCond_Once, ImVec2(0.02f, 0.5f));
    ImGui::SetNextWindowBgAlpha(0.1f);
    ImGui::Begin("Watermark", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollbar);

    ImGui::Text("Application ShitBox %.3f ms/frame (%.1f FPS)", 1000.0f / io.Framerate, io.Framerate);
    WindowSize = ImGui::GetWindowSize();
    WindowPos = ImGui::GetWindowPos();
    ImGui::End();
}