#include "../include/AccountWidget.hpp"
#include "../include/AssetManager.hpp"

#include <imgui.h>

void AccountWidget::performImGui()
{
    ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f));
    ImVec2 screen = ImGui::GetIO().DisplaySize;

    float width = screen.x * 0.10f;
    float height = width * 9.0f / 16.0f;

    ImGui::SetNextWindowSize(ImVec2(width, height));

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
                              ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings;

    ImGui::Begin("account_preview", nullptr, flags);

    

    if(editing)
    {
        ImGui::InputText("##input", input, 16);
        ImGui::SameLine();
        
        if(ImGui::Button("Confirm"))
        {
            editing = false;

            Account::username = input;
        }        
    }
    else
    {
        ImGui::TextUnformatted(Account::username.c_str());
        ImGui::SameLine();
        editing = ImGui::Button("Edit");
    }


    ImGui::End();
}