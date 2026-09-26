#include "../include/RCon.hpp"
#include <imgui.h>

void RCon::handleEvent(const sf::Event& event)
{
    if(event.is<sf::Event::KeyPressed>() && event.getIf<sf::Event::KeyPressed>()->code == sf::Keyboard::Key::F1) open = !open;
}

void RCon::performImGui(GameServer& server)
{
    if(!open) return;

    if(ImGui::Begin("RCon", &open, ImGuiWindowFlags_MenuBar))
    {
        if(ImGui::BeginMenuBar())
        {
            if(ImGui::BeginMenu("Server"))
            {
                ImGui::EndMenu();
            }

            if(ImGui::BeginMenu("Entities"))
            {
                ImGui::EndMenu();
            }

            if(ImGui::BeginMenu("Chunks"))
            {
                ImGui::EndMenu();
            }

            ImGui::EndMenuBar();
        }

        ImGui::End();
    }    
}