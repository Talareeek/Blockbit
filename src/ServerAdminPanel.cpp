#include "../include/ServerAdminPanel.hpp"

#include "../include/Packet.hpp"

#include <imgui.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <limits>
#include <string>
#include <vector>

ServerAdminPanel::ServerAdminPanel(std::unique_ptr<AdminSession> session) : session(std::move(session))
{

}

void ServerAdminPanel::toggle()
{
    open = !open;
    if (open)
    {
        requestList();
        requestBans();
        requestOperators();
    }
}

void ServerAdminPanel::handleEvent(const sf::Event& event)
{
    if (event.is<sf::Event::KeyPressed>() &&
        event.getIf<sf::Event::KeyPressed>()->code == sf::Keyboard::Key::F5)
    {
        toggle();
    }
}

void ServerAdminPanel::onAdminResponsePacket(PacketReader& reader)
{
    if (session) session->onResponsePacket(reader);
}

void ServerAdminPanel::requestList()
{
    if (!session || list_pending) return;
    list_pending = true;
    session->listEntities([this](bool authorized, std::vector<AdminEntityDescriptor> list)
    {
        list_pending = false;
        list_received = true;
        if (!authorized)
        {
            entities.clear();
            status_line = "Not authorized.";
            return;
        }
        entities = std::move(list);
    });
}

void ServerAdminPanel::requestBans()
{
    if (!session || bans_pending) return;
    bans_pending = true;
    session->listBans([this](bool authorized, std::vector<AdminBanDescriptor> list)
    {
        bans_pending = false;
        bans_received = true;
        if (!authorized)
        {
            bans.clear();
            status_line = "Not authorized.";
            return;
        }
        bans = std::move(list);
    });
}

void ServerAdminPanel::requestOperators()
{
    if (!session || operators_pending) return;
    operators_pending = true;
    session->listOperators([this](bool authorized, std::vector<std::string> list)
    {
        operators_pending = false;
        operators_received = true;
        if (!authorized)
        {
            operators.clear();
            status_line = "Not authorized.";
            return;
        }
        operators = std::move(list);
    });
}

void ServerAdminPanel::requestEntity(UUID id)
{
    if (!session) return;
    working_copy.reset();
    working_copy_dirty = false;
    working_copy_pending = true;
    session->readEntity(id, [this, id](bool authorized, std::optional<Tag> tag)
    {
        working_copy_pending = false;
        if (!authorized)
        {
            status_line = "Not authorized.";
            working_copy.reset();
            return;
        }
        if (!tag)
        {
            status_line = "Entity disappeared.";
            selected_entity.reset();
            working_copy.reset();
            return;
        }
        if (selected_entity && *selected_entity == id)
        {
            working_copy = std::move(tag);
        }
    });
}

void ServerAdminPanel::applyWorkingCopy()
{
    if (!session || !selected_entity || !working_copy) return;
    write_pending = true;
    UUID id = *selected_entity;
    Tag payload = *working_copy;
    session->writeEntity(id, payload, [this](bool authorized, bool ok)
    {
        write_pending = false;
        if (!authorized)
        {
            status_line = "Not authorized.";
            return;
        }
        if (!ok)
        {
            status_line = "Apply failed (entity gone?).";
            return;
        }
        status_line = "Applied.";
        working_copy_dirty = false;
    });
}

// ---------- BBT editor ----------

namespace
{

bool renderTagEditor(const char* label, Tag& tag);

template<typename Int>
bool dragScalarInt(const char* label, Int& value, ImGuiDataType type)
{
    return ImGui::DragScalar(label, type, &value, 1.0f);
}

bool renderCompoundEditor(const char* label, TagCompound& compound)
{
    bool changed = false;

    if (!ImGui::TreeNodeEx(label, ImGuiTreeNodeFlags_DefaultOpen))
    {
        return false;
    }

    std::vector<std::string> keys;
    keys.reserve(compound.size());
    for (const auto& [k, _] : compound) keys.push_back(k);
    std::sort(keys.begin(), keys.end());

    for (const auto& key : keys)
    {
        ImGui::PushID(key.c_str());
        if (renderTagEditor(key.c_str(), compound.at(key))) changed = true;
        ImGui::PopID();
    }

    ImGui::TreePop();
    return changed;
}

bool renderListEditor(const char* label, TagList& list)
{
    bool changed = false;

    std::string header = std::string(label) + " [" + std::to_string(list.size()) + "]";
    if (!ImGui::TreeNodeEx(header.c_str(), ImGuiTreeNodeFlags_DefaultOpen))
    {
        return false;
    }

    for (size_t i = 0; i < list.size(); ++i)
    {
        ImGui::PushID(static_cast<int>(i));
        std::string element_label = "[" + std::to_string(i) + "]";
        if (renderTagEditor(element_label.c_str(), list[i])) changed = true;
        ImGui::PopID();
    }

    ImGui::TreePop();
    return changed;
}

bool renderTagEditor(const char* label, Tag& tag)
{
    switch (tag.type())
    {
        case TagType::T_END:
            ImGui::TextDisabled("%s: <end>", label);
            return false;

        case TagType::T_BOOL:
        {
            bool v = tag.get<bool>();
            if (ImGui::Checkbox(label, &v)) { tag = Tag(v); return true; }
            return false;
        }
        case TagType::T_INT8:
        {
            int v = tag.get<int8_t>();
            if (ImGui::DragInt(label, &v, 1.0f,
                               std::numeric_limits<int8_t>::min(),
                               std::numeric_limits<int8_t>::max()))
            {
                tag = Tag(static_cast<int8_t>(v));
                return true;
            }
            return false;
        }
        case TagType::T_INT16:
        {
            int v = tag.get<int16_t>();
            if (ImGui::DragInt(label, &v, 1.0f,
                               std::numeric_limits<int16_t>::min(),
                               std::numeric_limits<int16_t>::max()))
            {
                tag = Tag(static_cast<int16_t>(v));
                return true;
            }
            return false;
        }
        case TagType::T_INT32:
        {
            int32_t v = tag.get<int32_t>();
            if (dragScalarInt(label, v, ImGuiDataType_S32)) { tag = Tag(v); return true; }
            return false;
        }
        case TagType::T_INT64:
        {
            int64_t v = tag.get<int64_t>();
            if (dragScalarInt(label, v, ImGuiDataType_S64)) { tag = Tag(v); return true; }
            return false;
        }
        case TagType::T_UINT8:
        {
            uint8_t v = tag.get<uint8_t>();
            if (dragScalarInt(label, v, ImGuiDataType_U8)) { tag = Tag(v); return true; }
            return false;
        }
        case TagType::T_UINT16:
        {
            uint16_t v = tag.get<uint16_t>();
            if (dragScalarInt(label, v, ImGuiDataType_U16)) { tag = Tag(v); return true; }
            return false;
        }
        case TagType::T_UINT32:
        {
            uint32_t v = tag.get<uint32_t>();
            if (dragScalarInt(label, v, ImGuiDataType_U32)) { tag = Tag(v); return true; }
            return false;
        }
        case TagType::T_UINT64:
        {
            uint64_t v = tag.get<uint64_t>();
            if (dragScalarInt(label, v, ImGuiDataType_U64)) { tag = Tag(v); return true; }
            return false;
        }
        case TagType::T_FLOAT:
        {
            float v = tag.get<float>();
            if (ImGui::DragFloat(label, &v, 0.05f)) { tag = Tag(v); return true; }
            return false;
        }
        case TagType::T_DOUBLE:
        {
            double v = tag.get<double>();
            if (ImGui::InputDouble(label, &v)) { tag = Tag(v); return true; }
            return false;
        }
        case TagType::T_STRING:
        {
            std::string v = tag.get<std::string>();
            char buffer[256];
            std::snprintf(buffer, sizeof(buffer), "%s", v.c_str());
            if (ImGui::InputText(label, buffer, sizeof(buffer)))
            {
                tag = Tag(std::string(buffer));
                return true;
            }
            return false;
        }
        case TagType::T_VECTOR2_FLOAT:
        {
            sf::Vector2f v = tag.get<sf::Vector2f>();
            float arr[2] = { v.x, v.y };
            if (ImGui::DragFloat2(label, arr, 0.05f))
            {
                tag = Tag(sf::Vector2f{ arr[0], arr[1] });
                return true;
            }
            return false;
        }
        case TagType::T_VECTOR2_INT:
        {
            sf::Vector2i v = tag.get<sf::Vector2i>();
            int arr[2] = { v.x, v.y };
            if (ImGui::DragInt2(label, arr, 1.0f))
            {
                tag = Tag(sf::Vector2i{ arr[0], arr[1] });
                return true;
            }
            return false;
        }
        case TagType::T_VECTOR2_DOUBLE:
        {
            sf::Vector2<double> v = tag.get<sf::Vector2<double>>();
            double arr[2] = { v.x, v.y };
            if (ImGui::InputScalarN(label, ImGuiDataType_Double, arr, 2))
            {
                tag = Tag(sf::Vector2<double>{ arr[0], arr[1] });
                return true;
            }
            return false;
        }
        case TagType::T_BYTEARRAY:
        {
            const auto& bytes = tag.get<std::vector<uint8_t>>();
            ImGui::TextDisabled("%s: byte[%zu] (read-only)", label, bytes.size());
            return false;
        }
        case TagType::T_LIST:
            return renderListEditor(label, tag.get<TagList>());
        case TagType::T_COMPOUND:
            return renderCompoundEditor(label, tag.get<TagCompound>());
    }
    return false;
}

} // namespace

void ServerAdminPanel::drawEntitiesTab()
{
    if (!session)
    {
        ImGui::TextDisabled("No admin session.");
        return;
    }

    if (!session->isAuthorized())
    {
        ImGui::TextDisabled("Session is not authorized.");
        ImGui::TextDisabled("Only operators can use the admin panel over the network.");
        return;
    }

    ImGui::BeginChild("##EntityList", ImVec2(260, 0), true);
    if (ImGui::Button("Refresh list"))
    {
        requestList();
    }
    if (list_pending)
    {
        ImGui::SameLine();
        ImGui::TextDisabled("loading…");
    }
    ImGui::Separator();

    if (!list_received && !list_pending)
    {
        ImGui::TextDisabled("No data yet.");
    }

    for (const auto& descriptor : entities)
    {
        bool is_selected = selected_entity && *selected_entity == descriptor.id;
        std::string label = descriptor.label + "##" + descriptor.id.toString();
        if (ImGui::Selectable(label.c_str(), is_selected))
        {
            selected_entity = descriptor.id;
            requestEntity(descriptor.id);
        }
    }
    ImGui::EndChild();

    ImGui::SameLine();

    ImGui::BeginChild("##EntityEditor", ImVec2(0, 0), true);

    if (!selected_entity)
    {
        ImGui::TextDisabled("Select an entity on the left.");
        ImGui::EndChild();
        return;
    }

    ImGui::TextUnformatted(selected_entity->toString().c_str());
    ImGui::Separator();

    if (ImGui::Button("Reload"))
    {
        requestEntity(*selected_entity);
    }
    ImGui::SameLine();

    ImGui::BeginDisabled(!working_copy_dirty || !working_copy || write_pending);
    if (ImGui::Button("Apply"))
    {
        applyWorkingCopy();
    }
    ImGui::EndDisabled();

    if (write_pending)
    {
        ImGui::SameLine();
        ImGui::TextDisabled("applying…");
    }
    else if (!status_line.empty())
    {
        ImGui::SameLine();
        ImGui::TextDisabled("%s", status_line.c_str());
    }

    ImGui::Separator();

    if (working_copy_pending)
    {
        ImGui::TextDisabled("loading entity…");
        ImGui::EndChild();
        return;
    }

    if (!working_copy)
    {
        ImGui::TextDisabled("Entity not found.");
        ImGui::EndChild();
        return;
    }

    // Working copy is always a compound at the top level (Entity::serialize()
    // returns a compound keyed by component name). Render each component as
    // its own collapsible subtree.
    TagCompound& components = working_copy->get<TagCompound>();
    std::vector<std::string> component_names;
    component_names.reserve(components.size());
    for (const auto& [k, _] : components) component_names.push_back(k);
    std::sort(component_names.begin(), component_names.end());

    for (const auto& name : component_names)
    {
        ImGui::PushID(name.c_str());
        if (ImGui::CollapsingHeader(name.c_str(), ImGuiTreeNodeFlags_DefaultOpen))
        {
            Tag& component_tag = components.at(name);
            if (component_tag.type() == TagType::T_COMPOUND)
            {
                TagCompound& fields = component_tag.get<TagCompound>();
                std::vector<std::string> field_names;
                field_names.reserve(fields.size());
                for (const auto& [k, _] : fields) field_names.push_back(k);
                std::sort(field_names.begin(), field_names.end());

                for (const auto& field_name : field_names)
                {
                    ImGui::PushID(field_name.c_str());
                    if (renderTagEditor(field_name.c_str(), fields.at(field_name)))
                    {
                        working_copy_dirty = true;
                        status_line.clear();
                    }
                    ImGui::PopID();
                }
            }
            else
            {
                if (renderTagEditor(name.c_str(), component_tag))
                {
                    working_copy_dirty = true;
                    status_line.clear();
                }
            }
        }
        ImGui::PopID();
    }

    ImGui::EndChild();
}

void ServerAdminPanel::drawBansTab()
{
    if (!session)
    {
        ImGui::TextDisabled("No admin session.");
        return;
    }
    if (!session->isAuthorized())
    {
        ImGui::TextDisabled("Session is not authorized.");
        return;
    }

    if (ImGui::Button("Refresh bans"))
    {
        requestBans();
    }
    if (bans_pending)
    {
        ImGui::SameLine();
        ImGui::TextDisabled("loading…");
    }
    ImGui::Separator();

    ImGui::TextUnformatted("Ban player");
    ImGui::SetNextItemWidth(160);
    ImGui::InputText("##BanNick", ban_input_nickname, sizeof(ban_input_nickname));
    ImGui::SameLine();
    ImGui::SetNextItemWidth(240);
    ImGui::InputText("##BanReason", ban_input_reason, sizeof(ban_input_reason));
    ImGui::SameLine();
    bool can_ban = ban_input_nickname[0] != '\0' && !ban_mutation_pending;
    ImGui::BeginDisabled(!can_ban);
    if (ImGui::Button("Ban"))
    {
        std::string nick = ban_input_nickname;
        std::string reason = ban_input_reason;
        ban_mutation_pending = true;
        session->addBan(nick, reason, [this, nick](bool authorized, bool ok)
        {
            ban_mutation_pending = false;
            if (!authorized) { status_line = "Not authorized."; return; }
            if (!ok)          { status_line = "Ban failed.";     return; }
            status_line = "Banned " + nick + ".";
            ban_input_nickname[0] = '\0';
            ban_input_reason[0]   = '\0';
            requestBans();
        });
    }
    ImGui::EndDisabled();
    if (ban_mutation_pending)
    {
        ImGui::SameLine();
        ImGui::TextDisabled("applying…");
    }
    else if (!status_line.empty())
    {
        ImGui::SameLine();
        ImGui::TextDisabled("%s", status_line.c_str());
    }
    ImGui::Separator();

    if (!bans_received && !bans_pending)
    {
        ImGui::TextDisabled("No data yet.");
        return;
    }
    if (bans.empty())
    {
        ImGui::TextDisabled("No banned players.");
        return;
    }

    if (ImGui::BeginTable("##BansTable", 4,
                          ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                          ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY))
    {
        ImGui::TableSetupColumn("Nickname");
        ImGui::TableSetupColumn("Banned at");
        ImGui::TableSetupColumn("Reason");
        ImGui::TableSetupColumn("##Actions", ImGuiTableColumnFlags_WidthFixed, 80.0f);
        ImGui::TableHeadersRow();

        for (const auto& entry : bans)
        {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::TextUnformatted(entry.nickname.c_str());
            ImGui::TableSetColumnIndex(1);
            if (entry.banned_at.empty()) ImGui::TextDisabled("—");
            else ImGui::TextUnformatted(entry.banned_at.c_str());
            ImGui::TableSetColumnIndex(2);
            if (entry.reason.empty()) ImGui::TextDisabled("—");
            else ImGui::TextUnformatted(entry.reason.c_str());
            ImGui::TableSetColumnIndex(3);

            ImGui::PushID(entry.nickname.c_str());
            ImGui::BeginDisabled(ban_mutation_pending);
            if (ImGui::SmallButton("Unban"))
            {
                std::string nick = entry.nickname;
                ban_mutation_pending = true;
                session->removeBan(nick, [this, nick](bool authorized, bool ok)
                {
                    ban_mutation_pending = false;
                    if (!authorized) { status_line = "Not authorized."; return; }
                    if (!ok)          { status_line = "Unban failed.";   return; }
                    status_line = "Unbanned " + nick + ".";
                    requestBans();
                });
            }
            ImGui::EndDisabled();
            ImGui::PopID();
        }
        ImGui::EndTable();
    }
}

void ServerAdminPanel::drawOperatorsTab()
{
    if (!session)
    {
        ImGui::TextDisabled("No admin session.");
        return;
    }
    if (!session->isAuthorized())
    {
        ImGui::TextDisabled("Session is not authorized.");
        return;
    }

    if (ImGui::Button("Refresh operators"))
    {
        requestOperators();
    }
    if (operators_pending)
    {
        ImGui::SameLine();
        ImGui::TextDisabled("loading…");
    }
    ImGui::Separator();

    ImGui::TextUnformatted("Add operator");
    ImGui::SetNextItemWidth(200);
    ImGui::InputText("##OpNick", operator_input_nickname, sizeof(operator_input_nickname));
    ImGui::SameLine();
    bool can_add = operator_input_nickname[0] != '\0' && !operator_mutation_pending;
    ImGui::BeginDisabled(!can_add);
    if (ImGui::Button("Grant"))
    {
        std::string nick = operator_input_nickname;
        operator_mutation_pending = true;
        session->addOperator(nick, [this, nick](bool authorized, bool ok)
        {
            operator_mutation_pending = false;
            if (!authorized) { status_line = "Not authorized."; return; }
            if (!ok)          { status_line = "Grant failed.";   return; }
            status_line = "Granted operator: " + nick + ".";
            operator_input_nickname[0] = '\0';
            requestOperators();
        });
    }
    ImGui::EndDisabled();
    if (operator_mutation_pending)
    {
        ImGui::SameLine();
        ImGui::TextDisabled("applying…");
    }
    ImGui::Separator();

    if (!operators_received && !operators_pending)
    {
        ImGui::TextDisabled("No data yet.");
        return;
    }
    if (operators.empty())
    {
        ImGui::TextDisabled("No operators.");
        return;
    }

    if (ImGui::BeginTable("##OperatorsTable", 2,
                          ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                          ImGuiTableFlags_ScrollY))
    {
        ImGui::TableSetupColumn("Nickname");
        ImGui::TableSetupColumn("##Actions", ImGuiTableColumnFlags_WidthFixed, 80.0f);
        ImGui::TableHeadersRow();

        for (const auto& nick_entry : operators)
        {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::TextUnformatted(nick_entry.c_str());
            ImGui::TableSetColumnIndex(1);

            ImGui::PushID(nick_entry.c_str());
            ImGui::BeginDisabled(operator_mutation_pending);
            if (ImGui::SmallButton("Revoke"))
            {
                std::string nick = nick_entry;
                operator_mutation_pending = true;
                session->removeOperator(nick, [this, nick](bool authorized, bool ok)
                {
                    operator_mutation_pending = false;
                    if (!authorized) { status_line = "Not authorized."; return; }
                    if (!ok)          { status_line = "Revoke failed.";  return; }
                    status_line = "Revoked operator: " + nick + ".";
                    requestOperators();
                });
            }
            ImGui::EndDisabled();
            ImGui::PopID();
        }
        ImGui::EndTable();
    }
}

void ServerAdminPanel::performImGui()
{
    if (session) session->update();
    if (!open) return;

    ImGui::SetNextWindowSize(ImVec2(720, 480), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Server Admin", &open))
    {
        if (ImGui::BeginTabBar("##AdminTabs"))
        {
            if (ImGui::BeginTabItem("Entities"))
            {
                drawEntitiesTab();
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Bans"))
            {
                drawBansTab();
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Operators"))
            {
                drawOperatorsTab();
                ImGui::EndTabItem();
            }
            ImGui::EndTabBar();
        }
    }
    ImGui::End();
}
