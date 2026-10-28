#include "../include/AccountWidget.hpp"
#include "../include/AssetManager.hpp"

#include <imgui.h>

void AccountWidget::performImGui(sf::Vector2f position, sf::Vector2f size)
{
    ImGui::SetNextWindowPos(ImVec2(position.x, position.y));
    ImGui::SetNextWindowSize(ImVec2(size.x, size.y));

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