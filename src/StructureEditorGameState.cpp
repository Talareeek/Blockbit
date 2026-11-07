#include "../include/StructureEditorGameState.hpp"
#include "../include/Game.hpp"
#include "../include/InputManager.hpp"
#include "../include/AssetManager.hpp"
#include "../include/BlockAtlas.hpp"
#include "../include/Block.hpp"
#include "../include/BlockStates.hpp"
#include "../include/Item.hpp"
#include "../include/BBT.hpp"
#include "../include/World.hpp"

#include <SFML/Graphics/RectangleShape.hpp>
#include <SFML/Graphics/Sprite.hpp>
#include <SFML/Graphics/CircleShape.hpp>
#include <SFML/Graphics/Text.hpp>

#include <imgui.h>
#include <imgui-SFML.h>

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>

namespace
{
    // Every BlockID the engine knows about. Order mirrors the enum declaration
    // in Block.hpp — if you add a block there, add it here too so the editor
    // palette stays complete. Air is included so the user can explicitly mark
    // "this cell MUST be air" via Overwrite mode.
    constexpr std::array<BlockID, 44> BLOCK_OPTIONS
    {
        BlockID::Air,
        BlockID::Stone,
        BlockID::Grass,
        BlockID::Dirt,
        BlockID::Cobblestone,
        BlockID::Obsidian,
        BlockID::Bedrock,
        BlockID::Water,
        BlockID::Lava,
        BlockID::Coal_Ore,
        BlockID::Iron_Ore,
        BlockID::Gold_Ore,
        BlockID::Diamond_Ore,
        BlockID::Ruby_Ore,
        BlockID::Iron_Block,
        BlockID::Gold_Block,
        BlockID::Ruby_Block,
        BlockID::Coal_Block,
        BlockID::Oak_Log,
        BlockID::Oak_Leaves,
        BlockID::Oak_Planks,
        BlockID::Birch_Log,
        BlockID::Birch_Leaves,
        BlockID::Spruce_Log,
        BlockID::Spruce_Leaves,
        BlockID::Woodcutter,
        BlockID::Fire,
        BlockID::Sand,
        BlockID::Coarse_Dirt,
        BlockID::Snow,
        BlockID::Chest,
        BlockID::Bricks,
        BlockID::Furnace,
        BlockID::Workbench,
        BlockID::Gravel,
        BlockID::Cactus,
        BlockID::Dead_Bush,
        BlockID::Smooth_Stone,
        BlockID::Diorite,
        BlockID::Granite,
        BlockID::Glass,
        BlockID::Output_Bulb,
        BlockID::Ruby_Wire,
        BlockID::Coreheart_Of_Blockbit
    };

    const char* blockName(BlockID id)
    {
        switch (id)
        {
            case BlockID::Air:   return "Air";
            case BlockID::Water: return "Water";
            case BlockID::Lava:  return "Lava";
            case BlockID::Fire:  return "Fire";
            default: break;
        }
        auto item = blockToItem(id);
        auto iter = itemDatabase.find(item);
        if (iter == itemDatabase.end()) return "?";
        return iter->second.name.c_str();
    }

    constexpr std::array<BlockEntityID, 2> BE_OPTIONS
    {
        BlockEntityID::Chest,
        BlockEntityID::Furnace,
    };

    const char* blockEntityName(BlockEntityID id)
    {
        switch (id)
        {
            case BlockEntityID::Chest:   return "Chest";
            case BlockEntityID::Furnace: return "Furnace";
            default:                     return "?";
        }
    }

    constexpr std::array<PrefabID, 5> SPAWN_OPTIONS
    {
        PrefabID::Pig,
        PrefabID::Dynamite,
        PrefabID::Cube,
        PrefabID::DroppedItem,
        PrefabID::FallingBlock,
    };

    const char* prefabName(PrefabID id)
    {
        switch (id)
        {
            case PrefabID::Pig:          return "Pig";
            case PrefabID::Dynamite:     return "Dynamite";
            case PrefabID::Cube:         return "Cube (boss)";
            case PrefabID::DroppedItem:  return "Dropped item";
            case PrefabID::FallingBlock: return "Falling block";
            default:                     return "?";
        }
    }

    const char* toolName(StructureEditorGameState::Tool tool)
    {
        using T = StructureEditorGameState::Tool;
        switch (tool)
        {
            case T::Paint:          return "Paint";
            case T::Erase:          return "Erase";
            case T::Pick:           return "Pick";
            case T::FillRect:       return "Fill rect";
            case T::SetAnchor:      return "Set anchor";
            case T::AddBlockEntity: return "Add block entity";
            case T::AddSpawn:       return "Add spawn";
            case T::RemoveMarker:   return "Remove marker";
        }
        return "?";
    }

    int paletteIndexOf(BlockID id)
    {
        for (std::size_t i = 0; i < BLOCK_OPTIONS.size(); i++)
            if (BLOCK_OPTIONS[i] == id) return static_cast<int>(i);
        return 0;
    }

    std::filesystem::path structuresDir()
    {
        // Lives next to saves (~/Blockbit/structures on Linux,
        // %APPDATA%/Blockbit/structures on Windows). Authored content is
        // user-owned, not an immutable asset shipped in resources/.
        return getStructuresPath();
    }

    const sf::Color SKIP_COLOR     {80, 80, 90, 255};   // charcoal — "pass through"
    const sf::Color AIR_ONLY_TINT  {80, 180, 255, 60};  // blue overlay (Overwrite gets no tint)
    const sf::Color GRID_COLOR     {40, 40, 50, 200};
    const sf::Color ANCHOR_COLOR   {255, 200, 50, 220};
    const sf::Color BE_COLOR       {200, 90, 220, 220};
    const sf::Color SPAWN_COLOR    {90, 230, 120, 220};
    const sf::Color HOVER_COLOR    {255, 255, 255, 60};
    const sf::Color DRAG_COLOR     {255, 220, 100, 80};
}

StructureEditorGameState::StructureEditorGameState(Game* game) : GameState(game)
{
    resetToBlank(16, 16);
    scanSavedFiles();

    canvas = sf::RenderTexture(canvas_size);
    refreshCanvas();
}

void StructureEditorGameState::resetToBlank(int w, int h)
{
    tmpl.width    = w;
    tmpl.height   = h;
    tmpl.anchor_x = w / 2;
    tmpl.anchor_y = 0;
    tmpl.cells.assign(static_cast<std::size_t>(w * h),
                      {Block{BlockID::Air, 0}, StructurePlacementMode::Skip});
    tmpl.block_entities.clear();
    tmpl.spawns.clear();
    current_name.clear();
    name_buffer[0] = '\0';
    dirty = false;
    pan_x = 0.0f;
    pan_y = 0.0f;
}

void StructureEditorGameState::resizeTemplate(int new_w, int new_h)
{
    if (new_w < 1) new_w = 1;
    if (new_h < 1) new_h = 1;
    if (new_w > 128) new_w = 128;
    if (new_h > 256) new_h = 256;

    if (new_w == tmpl.width && new_h == tmpl.height) return;

    std::vector<StructureCell> new_cells(static_cast<std::size_t>(new_w * new_h),
        {Block{BlockID::Air, 0}, StructurePlacementMode::Skip});

    const int copy_w = std::min(new_w, tmpl.width);
    const int copy_h = std::min(new_h, tmpl.height);

    // Keep cells anchored at (0,0) — the bottom-left corner — so resize feels
    // predictable regardless of anchor_x/anchor_y.
    for (int y = 0; y < copy_h; y++)
    {
        for (int x = 0; x < copy_w; x++)
        {
            new_cells[y * new_w + x] = tmpl.cells[y * tmpl.width + x];
        }
    }

    tmpl.width  = new_w;
    tmpl.height = new_h;
    tmpl.cells  = std::move(new_cells);

    // Clamp anchor and prune out-of-bounds markers.
    tmpl.anchor_x = std::clamp(tmpl.anchor_x, 0, new_w - 1);
    tmpl.anchor_y = std::clamp(tmpl.anchor_y, 0, new_h - 1);

    auto in_bounds = [&](int rx, int ry)
    {
        return rx >= 0 && rx < new_w && ry >= 0 && ry < new_h;
    };

    tmpl.block_entities.erase(
        std::remove_if(tmpl.block_entities.begin(), tmpl.block_entities.end(),
            [&](const StructureBlockEntity& be) { return !in_bounds(be.rx, be.ry); }),
        tmpl.block_entities.end());

    tmpl.spawns.erase(
        std::remove_if(tmpl.spawns.begin(), tmpl.spawns.end(),
            [&](const StructureSpawn& sp) { return !in_bounds(sp.rx, sp.ry); }),
        tmpl.spawns.end());

    dirty = true;
}

bool StructureEditorGameState::inBounds(int tx, int ty) const
{
    return tx >= 0 && tx < tmpl.width && ty >= 0 && ty < tmpl.height;
}

void StructureEditorGameState::scanSavedFiles()
{
    saved_files.clear();
    selected_saved_index = -1;

    std::error_code ec;
    std::filesystem::create_directories(structuresDir(), ec);

    for (const auto& entry : std::filesystem::directory_iterator(structuresDir(), ec))
    {
        if (!entry.is_regular_file()) continue;
        if (entry.path().extension() != ".bbt") continue;
        saved_files.push_back(entry.path().stem().string());
    }
    std::sort(saved_files.begin(), saved_files.end());
}

void StructureEditorGameState::saveCurrent()
{
    const std::string stem = name_buffer[0] ? name_buffer : current_name;
    if (stem.empty())
    {
        last_status = "Name is empty — enter a filename first.";
        return;
    }

    std::error_code ec;
    std::filesystem::create_directories(structuresDir(), ec);
    const std::filesystem::path path = structuresDir() / (stem + ".bbt");

    // Templates live inside a BBT "structure" container — one root compound
    // that mirrors StructureTemplate::serialize(). Extra metadata (name) is
    // wrapped at the outer level so we never touch the serialization format
    // the generator reads.
    Tag payload = tmpl.serialize();

    TagCompound root;
    root["name"]     = Tag(stem);
    root["template"] = payload;
    BBT bbt("structure", root);

    const std::vector<uint8_t> bytes = bbt.save();

    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out)
    {
        last_status = "Failed to open " + path.string();
        return;
    }
    out.write(reinterpret_cast<const char*>(bytes.data()),
              static_cast<std::streamsize>(bytes.size()));

    current_name = stem;
    dirty = false;
    last_status = "Saved to " + path.string();
    scanSavedFiles();
}

void StructureEditorGameState::loadFile(const std::string& stem)
{
    const std::filesystem::path path = structuresDir() / (stem + ".bbt");

    std::ifstream in(path, std::ios::binary);
    if (!in)
    {
        last_status = "Failed to open " + path.string();
        return;
    }

    std::vector<uint8_t> bytes(
        (std::istreambuf_iterator<char>(in)),
        std::istreambuf_iterator<char>());

    try
    {
        BBT bbt = BBT::load(bytes);
        const TagCompound& root = bbt.root();
        tmpl.deserialize(root.at("template"));

        current_name = stem;
        std::snprintf(name_buffer, sizeof(name_buffer), "%s", stem.c_str());
        dirty = false;
        last_status = "Loaded " + path.string();
        pan_x = 0.0f;
        pan_y = 0.0f;
    }
    catch (const std::exception& e)
    {
        last_status = std::string("Load failed: ") + e.what();
    }
}

void StructureEditorGameState::drawCellIntoCanvas(sf::RenderTexture& rt, int tx, int ty) const
{
    const StructureCell& cell = tmpl.cellAt(tx, ty);

    // Canvas-space position: y flipped so ty=0 is at the bottom.
    const float px = static_cast<float>(tx) * cell_pixels + pan_x;
    const float py = static_cast<float>(tmpl.height - 1 - ty) * cell_pixels + pan_y;

    const sf::Vector2f pos{px, py};
    const sf::Vector2f sz {cell_pixels, cell_pixels};

    // Background: solid dark for Skip; for non-Skip we lay down the block
    // texture (if the block has one in blockDatabase), then overlay the mode
    // tint.
    if (cell.mode == StructurePlacementMode::Skip)
    {
        sf::RectangleShape bg(sz);
        bg.setPosition(pos);
        bg.setFillColor(SKIP_COLOR);
        rt.draw(bg);
    }
    else
    {
        // Air shows as a soft blue — most authors expect "air" to be visible,
        // not invisible, otherwise they can't tell a painted-air cell apart
        // from a Skip cell.
        if (cell.block.id == BlockID::Air)
        {
            sf::RectangleShape bg(sz);
            bg.setPosition(pos);
            bg.setFillColor(sf::Color(30, 40, 70, 255));
            rt.draw(bg);
        }
        else
        {
            auto db_it = blockDatabase.find(cell.block.id);
            if (db_it != blockDatabase.end() && db_it->second.render.has_value())
            {
                const RenderProperties& render = *db_it->second.render;

                // Mirror the game's neighbor-aware sampling so connected
                // textures (smooth stone, coreheart) and state-driven sprites
                // (water level, chest open) preview exactly as placed. Cells
                // outside the template read as Air — same rule chunk borders
                // use in the live mesh builder.
                auto id_at = [&](int ox, int oy) -> BlockID
                {
                    if (!inBounds(ox, oy)) return BlockID::Air;
                    return tmpl.cellAt(ox, oy).block.id;
                };
                const BlockNeighbors neighbors{
                    id_at(tx,     ty + 1),
                    id_at(tx + 1, ty),
                    id_at(tx,     ty - 1),
                    id_at(tx - 1, ty),
                };

                const sf::IntRect atlas_uv  = BlockAtlas::getUV(render.texture);
                const sf::IntRect block_rect = render.rect(cell.block, neighbors);
                const sf::IntRect final_uv{
                    atlas_uv.position + block_rect.position,
                    block_rect.size
                };

                const sf::FloatRect bounds = render.render_bounds(cell.block);

                // Game uses Y-up world coords with a Y-flipped view; the editor
                // canvas is plain Y-down. Convert bounds.y=0 ("bottom of cell"
                // in-world) to screen-bottom here so partial-height blocks like
                // water sit at the floor of the cell, not the ceiling.
                const float draw_x = px + bounds.position.x * cell_pixels;
                const float draw_y = py + (1.0f - bounds.position.y - bounds.size.y) * cell_pixels;
                const float draw_w = bounds.size.x * cell_pixels;
                const float draw_h = bounds.size.y * cell_pixels;

                if (final_uv.size.x > 0 && final_uv.size.y > 0 && draw_w > 0.0f && draw_h > 0.0f)
                {
                    sf::Sprite sprite(BlockAtlas::getTexture());
                    sprite.setTextureRect(final_uv);
                    sprite.setPosition({draw_x, draw_y});
                    sprite.setScale({
                        draw_w / static_cast<float>(final_uv.size.x),
                        draw_h / static_cast<float>(final_uv.size.y),
                    });
                    rt.draw(sprite);
                }
                else
                {
                    sf::RectangleShape bg(sz);
                    bg.setPosition(pos);
                    bg.setFillColor(sf::Color(120, 120, 140));
                    rt.draw(bg);
                }
            }
            else
            {
                sf::RectangleShape bg(sz);
                bg.setPosition(pos);
                bg.setFillColor(sf::Color(120, 120, 140));
                rt.draw(bg);
            }
        }

        if (cell.mode == StructurePlacementMode::AirOnly)
        {
            sf::RectangleShape tint(sz);
            tint.setPosition(pos);
            tint.setFillColor(AIR_ONLY_TINT);
            rt.draw(tint);
        }
    }

    // Grid line on the right + bottom of the cell. Drawing only two edges per
    // cell avoids double-drawn lines without needing a pass over the whole
    // canvas.
    sf::RectangleShape right({1.0f, sz.y});
    right.setPosition({px + sz.x - 1.0f, py});
    right.setFillColor(GRID_COLOR);
    rt.draw(right);

    sf::RectangleShape bottom({sz.x, 1.0f});
    bottom.setPosition({px, py + sz.y - 1.0f});
    bottom.setFillColor(GRID_COLOR);
    rt.draw(bottom);
}

void StructureEditorGameState::refreshCanvas()
{
    // Resize the render texture if the window changed enough. We resize to
    // snap to multiples of cell_pixels so partial cells never appear on the
    // right/bottom edges.
    const unsigned need_w = static_cast<unsigned>(std::max(canvas_display_size.x, 64.0f));
    const unsigned need_h = static_cast<unsigned>(std::max(canvas_display_size.y, 64.0f));
    if (canvas.getSize().x != need_w || canvas.getSize().y != need_h)
    {
        canvas = sf::RenderTexture(sf::Vector2u(need_w, need_h));
        canvas_size = canvas.getSize();
    }

    canvas.clear(sf::Color(22, 22, 28));

    // Backdrop beyond template bounds — leave the clear color visible so the
    // user can tell where the template ends. Just draw the cells plus overlays.
    for (int ty = 0; ty < tmpl.height; ty++)
    {
        for (int tx = 0; tx < tmpl.width; tx++)
        {
            drawCellIntoCanvas(canvas, tx, ty);
        }
    }

    // Anchor marker — small cross at the center of the anchor cell.
    if (inBounds(tmpl.anchor_x, tmpl.anchor_y))
    {
        const float cx = (tmpl.anchor_x + 0.5f) * cell_pixels + pan_x;
        const float cy = (tmpl.height - 1 - tmpl.anchor_y + 0.5f) * cell_pixels + pan_y;

        const float r = cell_pixels * 0.35f;
        sf::RectangleShape h({r * 2.0f, 2.0f});
        h.setOrigin({r, 1.0f});
        h.setPosition({cx, cy});
        h.setFillColor(ANCHOR_COLOR);
        canvas.draw(h);

        sf::RectangleShape v({2.0f, r * 2.0f});
        v.setOrigin({1.0f, r});
        v.setPosition({cx, cy});
        v.setFillColor(ANCHOR_COLOR);
        canvas.draw(v);
    }

    // Block entity markers.
    for (const StructureBlockEntity& be : tmpl.block_entities)
    {
        if (!inBounds(be.rx, be.ry)) continue;
        const float cx = (be.rx + 0.5f) * cell_pixels + pan_x;
        const float cy = (tmpl.height - 1 - be.ry + 0.5f) * cell_pixels + pan_y;
        const float r = cell_pixels * 0.30f;
        sf::CircleShape circle(r);
        circle.setOrigin({r, r});
        circle.setPosition({cx, cy});
        circle.setFillColor(BE_COLOR);
        canvas.draw(circle);
    }

    // Spawn markers.
    for (const StructureSpawn& sp : tmpl.spawns)
    {
        if (!inBounds(sp.rx, sp.ry)) continue;
        const float cx = (sp.rx + 0.5f) * cell_pixels + pan_x;
        const float cy = (tmpl.height - 1 - sp.ry + 0.5f) * cell_pixels + pan_y;
        const float r = cell_pixels * 0.30f;
        sf::CircleShape diamond(r, 4);
        diamond.setOrigin({r, r});
        diamond.setPosition({cx, cy});
        diamond.setRotation(sf::degrees(45.0f));
        diamond.setFillColor(SPAWN_COLOR);
        canvas.draw(diamond);
    }

    // Hover highlight.
    if (has_hover && inBounds(hover_tx, hover_ty))
    {
        const float px = static_cast<float>(hover_tx) * cell_pixels + pan_x;
        const float py = static_cast<float>(tmpl.height - 1 - hover_ty) * cell_pixels + pan_y;
        sf::RectangleShape hl({cell_pixels, cell_pixels});
        hl.setPosition({px, py});
        hl.setFillColor(HOVER_COLOR);
        canvas.draw(hl);
    }

    // Drag rect preview when using FillRect.
    if (dragging && tool == Tool::FillRect && has_hover)
    {
        const int x0 = std::min(drag_start_tx, hover_tx);
        const int y0 = std::min(drag_start_ty, hover_ty);
        const int x1 = std::max(drag_start_tx, hover_tx);
        const int y1 = std::max(drag_start_ty, hover_ty);

        const float px = static_cast<float>(x0) * cell_pixels + pan_x;
        const float py = static_cast<float>(tmpl.height - 1 - y1) * cell_pixels + pan_y;
        const float w  = static_cast<float>(x1 - x0 + 1) * cell_pixels;
        const float h  = static_cast<float>(y1 - y0 + 1) * cell_pixels;

        sf::RectangleShape rect({w, h});
        rect.setPosition({px, py});
        rect.setFillColor(DRAG_COLOR);
        canvas.draw(rect);
    }

    canvas.display();
}

void StructureEditorGameState::handleCanvasClick(int tx, int ty, bool /*shift_down*/)
{
    if (!inBounds(tx, ty)) return;

    switch (tool)
    {
        case Tool::Paint:
        {
            StructureCell& cell = tmpl.cells[ty * tmpl.width + tx];
            cell.block = Block{BLOCK_OPTIONS[palette_index], paint_metadata};
            cell.mode  = paint_mode;
            dirty = true;
            break;
        }
        case Tool::Erase:
        {
            StructureCell& cell = tmpl.cells[ty * tmpl.width + tx];
            cell.block = Block{BlockID::Air, 0};
            cell.mode  = StructurePlacementMode::Skip;
            auto be_end = std::remove_if(tmpl.block_entities.begin(), tmpl.block_entities.end(),
                [&](const StructureBlockEntity& be) { return be.rx == tx && be.ry == ty; });
            tmpl.block_entities.erase(be_end, tmpl.block_entities.end());
            auto sp_end = std::remove_if(tmpl.spawns.begin(), tmpl.spawns.end(),
                [&](const StructureSpawn& sp) { return sp.rx == tx && sp.ry == ty; });
            tmpl.spawns.erase(sp_end, tmpl.spawns.end());
            dirty = true;
            break;
        }
        case Tool::Pick:
        {
            const StructureCell& cell = tmpl.cellAt(tx, ty);
            palette_index = paletteIndexOf(cell.block.id);
            paint_metadata = cell.block.metadata;
            if (cell.mode != StructurePlacementMode::Skip)
                paint_mode = cell.mode;
            break;
        }
        case Tool::SetAnchor:
        {
            tmpl.anchor_x = tx;
            tmpl.anchor_y = ty;
            dirty = true;
            break;
        }
        case Tool::AddBlockEntity:
        {
            // Replace any existing block entity on this cell.
            auto it = std::find_if(tmpl.block_entities.begin(), tmpl.block_entities.end(),
                [&](const StructureBlockEntity& be) { return be.rx == tx && be.ry == ty; });
            const BlockEntityID kind = BE_OPTIONS[block_entity_kind_index];
            if (it != tmpl.block_entities.end())
                it->id = kind;
            else
                tmpl.block_entities.push_back({tx, ty, kind});
            dirty = true;
            break;
        }
        case Tool::AddSpawn:
        {
            tmpl.spawns.push_back({tx, ty, SPAWN_OPTIONS[spawn_prefab_index]});
            dirty = true;
            break;
        }
        case Tool::RemoveMarker:
        {
            auto be_it = std::remove_if(tmpl.block_entities.begin(), tmpl.block_entities.end(),
                [&](const StructureBlockEntity& be) { return be.rx == tx && be.ry == ty; });
            if (be_it != tmpl.block_entities.end())
            {
                tmpl.block_entities.erase(be_it, tmpl.block_entities.end());
                dirty = true;
            }
            auto sp_it = std::remove_if(tmpl.spawns.begin(), tmpl.spawns.end(),
                [&](const StructureSpawn& sp) { return sp.rx == tx && sp.ry == ty; });
            if (sp_it != tmpl.spawns.end())
            {
                tmpl.spawns.erase(sp_it, tmpl.spawns.end());
                dirty = true;
            }
            break;
        }
        case Tool::FillRect:
        {
            // Nothing on click — fill happens on release. The drag preview is
            // handled by refreshCanvas based on drag_start / hover state.
            break;
        }
    }
}

void StructureEditorGameState::handleFillRelease(int ax, int ay, int bx, int by)
{
    const int x0 = std::clamp(std::min(ax, bx), 0, tmpl.width - 1);
    const int y0 = std::clamp(std::min(ay, by), 0, tmpl.height - 1);
    const int x1 = std::clamp(std::max(ax, bx), 0, tmpl.width - 1);
    const int y1 = std::clamp(std::max(ay, by), 0, tmpl.height - 1);

    for (int y = y0; y <= y1; y++)
    {
        for (int x = x0; x <= x1; x++)
        {
            StructureCell& cell = tmpl.cells[y * tmpl.width + x];
            cell.block = Block{BLOCK_OPTIONS[palette_index], paint_metadata};
            cell.mode  = paint_mode;
        }
    }
    dirty = true;
}

void StructureEditorGameState::drawLeftPanel()
{
    ImGui::BeginChild("##left", ImVec2(260.0f, 0.0f), true);

    ImGui::TextUnformatted("File");
    ImGui::Separator();

    ImGui::InputText("Name", name_buffer, sizeof(name_buffer));

    if (ImGui::Button("New"))
    {
        if (dirty)
        {
            pending = PendingAction::NewTemplate;
            show_confirm_discard = true;
        }
        else
        {
            resetToBlank(16, 16);
            last_status = "New template";
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Save")) saveCurrent();
    ImGui::SameLine();
    if (ImGui::Button("Refresh list")) scanSavedFiles();

    ImGui::TextDisabled("Saved files:");
    ImGui::BeginChild("##files", ImVec2(0.0f, 100.0f), true);
    for (int i = 0; i < static_cast<int>(saved_files.size()); i++)
    {
        const bool selected = (i == selected_saved_index);
        if (ImGui::Selectable(saved_files[i].c_str(), selected))
            selected_saved_index = i;
    }
    ImGui::EndChild();

    const bool can_load = selected_saved_index >= 0
                       && selected_saved_index < static_cast<int>(saved_files.size());
    if (!can_load) ImGui::BeginDisabled();
    if (ImGui::Button("Load selected"))
    {
        if (dirty)
        {
            pending = PendingAction::LoadSelected;
            show_confirm_discard = true;
        }
        else
        {
            loadFile(saved_files[selected_saved_index]);
        }
    }
    if (!can_load) ImGui::EndDisabled();

    ImGui::Separator();

    ImGui::TextUnformatted("Tool");
    const Tool tools[] =
    {
        Tool::Paint, Tool::Erase, Tool::Pick, Tool::FillRect,
        Tool::SetAnchor, Tool::AddBlockEntity, Tool::AddSpawn, Tool::RemoveMarker
    };
    for (Tool t : tools)
    {
        if (ImGui::RadioButton(toolName(t), tool == t))
            tool = t;
    }

    ImGui::Separator();
    ImGui::TextUnformatted("Palette");

    if (ImGui::BeginCombo("Block", blockName(BLOCK_OPTIONS[palette_index])))
    {
        for (int i = 0; i < static_cast<int>(BLOCK_OPTIONS.size()); i++)
        {
            const bool is_selected = (i == palette_index);
            if (ImGui::Selectable(blockName(BLOCK_OPTIONS[i]), is_selected))
            {
                palette_index = i;
                paint_metadata = 0;
            }
            if (is_selected) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }

    // Metadata editor — fields depend on the selected block. Sliders use the
    // same packed layout as BlockStates so the preview in drawCellIntoCanvas
    // matches what the live chunk mesh produces from the same metadata byte.
    const BlockID selected_id = BLOCK_OPTIONS[palette_index];
    switch (selected_id)
    {
        case BlockID::Water:
        {
            int level = WaterState::unpack(paint_metadata).level;
            if (ImGui::SliderInt("Water level", &level, 0, WaterState::SOURCE))
                paint_metadata = WaterState{static_cast<uint8_t>(level)}.pack();
            ImGui::TextDisabled("%d = source, %d = full flow", WaterState::SOURCE, WaterState::FULL);
            break;
        }
        case BlockID::Lava:
        {
            int level = LavaState::unpack(paint_metadata).level;
            if (ImGui::SliderInt("Lava level", &level, 0, LavaState::SOURCE))
                paint_metadata = LavaState{static_cast<uint8_t>(level)}.pack();
            ImGui::TextDisabled("%d = source, %d = full flow", LavaState::SOURCE, LavaState::FULL);
            break;
        }
        case BlockID::Chest:
        {
            bool open = ChestState::unpack(paint_metadata).open;
            if (ImGui::Checkbox("Open", &open))
                paint_metadata = ChestState{open}.pack();
            break;
        }
        case BlockID::Furnace:
        {
            bool lit = FurnaceState::unpack(paint_metadata).lit;
            if (ImGui::Checkbox("Lit", &lit))
                paint_metadata = FurnaceState{lit}.pack();
            break;
        }
        case BlockID::Cactus:
        {
            bool is_top = CactusState::unpack(paint_metadata).is_top;
            if (ImGui::Checkbox("Top segment", &is_top))
                paint_metadata = CactusState{is_top}.pack();
            break;
        }
        default:
        {
            int raw = static_cast<int>(paint_metadata);
            ImGui::SetNextItemWidth(100.0f);
            if (ImGui::InputInt("Metadata", &raw))
                paint_metadata = static_cast<uint16_t>(std::clamp(raw, 0, 0xFFFF));
            break;
        }
    }

    int mode_int = static_cast<int>(paint_mode);
    ImGui::TextUnformatted("Placement mode");
    ImGui::RadioButton("Overwrite (always stamp)", &mode_int,
        static_cast<int>(StructurePlacementMode::Overwrite));
    ImGui::RadioButton("Air only (don't touch terrain)", &mode_int,
        static_cast<int>(StructurePlacementMode::AirOnly));
    paint_mode = static_cast<StructurePlacementMode>(mode_int);

    ImGui::Separator();

    if (tool == Tool::AddBlockEntity)
    {
        ImGui::TextUnformatted("Block entity to place");
        if (ImGui::BeginCombo("##be", blockEntityName(BE_OPTIONS[block_entity_kind_index])))
        {
            for (int i = 0; i < static_cast<int>(BE_OPTIONS.size()); i++)
            {
                const bool is_selected = (i == block_entity_kind_index);
                if (ImGui::Selectable(blockEntityName(BE_OPTIONS[i]), is_selected))
                    block_entity_kind_index = i;
                if (is_selected) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
    }
    else if (tool == Tool::AddSpawn)
    {
        ImGui::TextUnformatted("Entity to spawn");
        if (ImGui::BeginCombo("##sp", prefabName(SPAWN_OPTIONS[spawn_prefab_index])))
        {
            for (int i = 0; i < static_cast<int>(SPAWN_OPTIONS.size()); i++)
            {
                const bool is_selected = (i == spawn_prefab_index);
                if (ImGui::Selectable(prefabName(SPAWN_OPTIONS[i]), is_selected))
                    spawn_prefab_index = i;
                if (is_selected) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
    }

    ImGui::Separator();
    if (ImGui::Button("Back to menu"))
    {
        if (dirty)
        {
            pending = PendingAction::LeaveEditor;
            show_confirm_discard = true;
        }
        else
        {
            should_close = true;
        }
    }

    ImGui::EndChild();
}

void StructureEditorGameState::drawRightPanel()
{
    ImGui::BeginChild("##right", ImVec2(260.0f, 0.0f), true);

    ImGui::TextUnformatted("Template");
    ImGui::Separator();

    int w = tmpl.width;
    int h = tmpl.height;
    bool resized = false;

    ImGui::SetNextItemWidth(100.0f);
    if (ImGui::InputInt("Width",  &w)) resized = true;
    ImGui::SetNextItemWidth(100.0f);
    if (ImGui::InputInt("Height", &h)) resized = true;
    if (resized) resizeTemplate(w, h);

    ImGui::SetNextItemWidth(100.0f);
    int ax = tmpl.anchor_x;
    if (ImGui::InputInt("Anchor X", &ax))
    {
        tmpl.anchor_x = std::clamp(ax, 0, tmpl.width - 1);
        dirty = true;
    }
    ImGui::SetNextItemWidth(100.0f);
    int ay = tmpl.anchor_y;
    if (ImGui::InputInt("Anchor Y", &ay))
    {
        tmpl.anchor_y = std::clamp(ay, 0, tmpl.height - 1);
        dirty = true;
    }

    ImGui::Separator();
    ImGui::Text("Block entities (%d)", static_cast<int>(tmpl.block_entities.size()));
    ImGui::BeginChild("##be_list", ImVec2(0.0f, 100.0f), true);
    int be_remove = -1;
    for (int i = 0; i < static_cast<int>(tmpl.block_entities.size()); i++)
    {
        const auto& be = tmpl.block_entities[i];
        ImGui::PushID(i);
        ImGui::Text("(%d,%d) %s", be.rx, be.ry, blockEntityName(be.id));
        ImGui::SameLine();
        if (ImGui::SmallButton("X")) be_remove = i;
        ImGui::PopID();
    }
    ImGui::EndChild();
    if (be_remove >= 0)
    {
        tmpl.block_entities.erase(tmpl.block_entities.begin() + be_remove);
        dirty = true;
    }

    ImGui::Text("Spawns (%d)", static_cast<int>(tmpl.spawns.size()));
    ImGui::BeginChild("##sp_list", ImVec2(0.0f, 100.0f), true);
    int sp_remove = -1;
    for (int i = 0; i < static_cast<int>(tmpl.spawns.size()); i++)
    {
        const auto& sp = tmpl.spawns[i];
        ImGui::PushID(1000 + i);
        ImGui::Text("(%d,%d) %s", sp.rx, sp.ry, prefabName(sp.prefab));
        ImGui::SameLine();
        if (ImGui::SmallButton("X")) sp_remove = i;
        ImGui::PopID();
    }
    ImGui::EndChild();
    if (sp_remove >= 0)
    {
        tmpl.spawns.erase(tmpl.spawns.begin() + sp_remove);
        dirty = true;
    }

    ImGui::Separator();
    ImGui::TextUnformatted("View");
    ImGui::SliderFloat("Zoom", &cell_pixels, 4.0f, 48.0f, "%.0f px");
    ImGui::SliderFloat("Pan X", &pan_x, -2000.0f, 2000.0f, "%.0f");
    ImGui::SliderFloat("Pan Y", &pan_y, -2000.0f, 2000.0f, "%.0f");
    if (ImGui::Button("Center view"))
    {
        pan_x = (canvas_display_size.x - tmpl.width * cell_pixels) * 0.5f;
        pan_y = (canvas_display_size.y - tmpl.height * cell_pixels) * 0.5f;
    }

    ImGui::EndChild();
}

void StructureEditorGameState::drawCanvasPanel()
{
    // Negative width reserves space on the right for drawRightPanel's child
    // (which is 260 px wide). Using (0,0) here eats the entire remaining
    // region and pushes the right panel off-screen, hiding the resize inputs.
    ImGui::BeginChild("##canvas", ImVec2(-270.0f, 0.0f), true);

    const ImVec2 avail = ImGui::GetContentRegionAvail();
    canvas_display_size = {avail.x, avail.y - 24.0f};
    if (canvas_display_size.x < 64.0f) canvas_display_size.x = 64.0f;
    if (canvas_display_size.y < 64.0f) canvas_display_size.y = 64.0f;

    refreshCanvas();

    const ImVec2 cursor = ImGui::GetCursorScreenPos();
    // Pass the RenderTexture itself (not .getTexture()) so imgui-sfml uses the
    // overload that flips UVs — RenderTexture's backing texture is Y-inverted
    // because of the OpenGL FBO, and the dedicated overload compensates. With
    // the plain sf::Texture overload the image shows upside-down and the
    // mouse→cell math picks the wrong row.
    ImGui::Image(canvas,
        sf::Vector2f(canvas_display_size.x, canvas_display_size.y));

    // Hover + click detection in canvas-local coords. ImGui::IsItemHovered
    // tracks the just-drawn image. Mouse position comes from ImGuiIO; both are
    // in screen pixels.
    const ImGuiIO& io = ImGui::GetIO();
    has_hover = false;

    if (ImGui::IsItemHovered())
    {
        const float lx = io.MousePos.x - cursor.x - pan_x;
        const float ly = io.MousePos.y - cursor.y - pan_y;

        const int tx = static_cast<int>(std::floor(lx / cell_pixels));
        const int ty_from_top = static_cast<int>(std::floor(ly / cell_pixels));
        const int ty = (tmpl.height - 1) - ty_from_top;

        if (inBounds(tx, ty))
        {
            has_hover = true;
            hover_tx = tx;
            hover_ty = ty;
        }

        // Zoom with scroll wheel.
        if (io.MouseWheel != 0.0f)
        {
            const float old_cell = cell_pixels;
            cell_pixels = std::clamp(cell_pixels + io.MouseWheel * 2.0f, 4.0f, 48.0f);
            // Keep the hovered cell under the cursor by compensating the pan.
            const float factor = cell_pixels / old_cell;
            pan_x = (pan_x - (io.MousePos.x - cursor.x)) * factor + (io.MousePos.x - cursor.x);
            pan_y = (pan_y - (io.MousePos.y - cursor.y)) * factor + (io.MousePos.y - cursor.y);
        }

        // Middle-drag pans.
        if (ImGui::IsMouseDragging(ImGuiMouseButton_Middle))
        {
            const ImVec2 delta = io.MouseDelta;
            pan_x += delta.x;
            pan_y += delta.y;
        }

        if (has_hover)
        {
            if (ImGui::IsMouseClicked(ImGuiMouseButton_Left))
            {
                if (tool == Tool::FillRect)
                {
                    dragging = true;
                    drag_start_tx = hover_tx;
                    drag_start_ty = hover_ty;
                }
                else
                {
                    handleCanvasClick(hover_tx, hover_ty, io.KeyShift);
                }
            }
            else if (ImGui::IsMouseDown(ImGuiMouseButton_Left) && tool == Tool::Paint)
            {
                handleCanvasClick(hover_tx, hover_ty, io.KeyShift);
            }
            else if (ImGui::IsMouseDown(ImGuiMouseButton_Left) && tool == Tool::Erase)
            {
                handleCanvasClick(hover_tx, hover_ty, io.KeyShift);
            }

            if (ImGui::IsMouseClicked(ImGuiMouseButton_Right))
            {
                // Right click is always erase, regardless of tool — matches the
                // mental model from in-game building. Clears any block entity
                // or spawn attached to the cell too, so the cell is fully blank.
                StructureCell& cell = tmpl.cells[hover_ty * tmpl.width + hover_tx];
                cell.block = Block{BlockID::Air, 0};
                cell.mode  = StructurePlacementMode::Skip;
                const int rx = hover_tx;
                const int ry = hover_ty;
                auto be_end = std::remove_if(tmpl.block_entities.begin(), tmpl.block_entities.end(),
                    [&](const StructureBlockEntity& be) { return be.rx == rx && be.ry == ry; });
                tmpl.block_entities.erase(be_end, tmpl.block_entities.end());
                auto sp_end = std::remove_if(tmpl.spawns.begin(), tmpl.spawns.end(),
                    [&](const StructureSpawn& sp) { return sp.rx == rx && sp.ry == ry; });
                tmpl.spawns.erase(sp_end, tmpl.spawns.end());
                dirty = true;
            }
        }
    }

    if (dragging && !ImGui::IsMouseDown(ImGuiMouseButton_Left))
    {
        if (tool == Tool::FillRect && has_hover)
            handleFillRelease(drag_start_tx, drag_start_ty, hover_tx, hover_ty);
        dragging = false;
    }

    drawStatusBar();

    ImGui::EndChild();
}

void StructureEditorGameState::drawStatusBar()
{
    ImGui::Separator();
    if (has_hover)
    {
        const StructureCell& cell = tmpl.cellAt(hover_tx, hover_ty);
        const char* mode_str =
            cell.mode == StructurePlacementMode::Skip      ? "Skip" :
            cell.mode == StructurePlacementMode::Overwrite ? "Overwrite" :
                                                             "AirOnly";
        ImGui::Text("(%d, %d)  cell=%s/%s  tool=%s  %s%s",
                    hover_tx, hover_ty,
                    blockName(cell.block.id), mode_str,
                    toolName(tool),
                    dirty ? "[modified] " : "",
                    last_status.c_str());
    }
    else
    {
        ImGui::Text("tool=%s  %s%s", toolName(tool),
                    dirty ? "[modified] " : "",
                    last_status.c_str());
    }
}

void StructureEditorGameState::drawDiscardConfirmation()
{
    if (!show_confirm_discard) return;

    ImGui::OpenPopup("Discard changes?");
    if (ImGui::BeginPopupModal("Discard changes?", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::TextUnformatted("The current template has unsaved changes.");
        ImGui::Separator();
        if (ImGui::Button("Discard", ImVec2(120, 0)))
        {
            show_confirm_discard = false;
            switch (pending)
            {
                case PendingAction::LeaveEditor:  should_close = true; break;
                case PendingAction::NewTemplate:  resetToBlank(16, 16); break;
                case PendingAction::LoadSelected:
                    if (selected_saved_index >= 0 &&
                        selected_saved_index < static_cast<int>(saved_files.size()))
                        loadFile(saved_files[selected_saved_index]);
                    break;
                default: break;
            }
            pending = PendingAction::None;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120, 0)))
        {
            show_confirm_discard = false;
            pending = PendingAction::None;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

void StructureEditorGameState::handleEvent(const sf::Event& /*event*/)
{
    // ImGui-SFML intercepts input at the Game level; nothing to do here.
}

void StructureEditorGameState::update(float /*dt*/)
{
    if (should_close)
    {
        game->popState(this);
        return;
    }

    if (InputManager::isLazyKeyPressed(sf::Keyboard::Key::Escape))
    {
        if (dirty)
        {
            pending = PendingAction::LeaveEditor;
            show_confirm_discard = true;
        }
        else
        {
            should_close = true;
            return;
        }
    }

    const ImVec2 screen = ImGui::GetIO().DisplaySize;
    ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f), ImGuiCond_Always);
    ImGui::SetNextWindowSize(screen, ImGuiCond_Always);

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoResize
                           | ImGuiWindowFlags_NoMove
                           | ImGuiWindowFlags_NoCollapse
                           | ImGuiWindowFlags_NoSavedSettings
                           | ImGuiWindowFlags_NoBringToFrontOnFocus;

    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.14f, 0.14f, 0.16f, 0.98f));

    bool open = true;
    if (ImGui::Begin("Structure editor", &open, flags))
    {
        drawLeftPanel();
        ImGui::SameLine();
        drawCanvasPanel();
        ImGui::SameLine();
        drawRightPanel();

        drawDiscardConfirmation();
    }
    ImGui::End();
    ImGui::PopStyleColor();

    if (!open)
    {
        if (dirty)
        {
            pending = PendingAction::LeaveEditor;
            show_confirm_discard = true;
        }
        else
        {
            should_close = true;
        }
    }
}

void StructureEditorGameState::render(sf::RenderWindow& window)
{
    window.setView(sf::View(sf::FloatRect({0.0f, 0.0f},
        {static_cast<float>(window.getSize().x), static_cast<float>(window.getSize().y)})));
    window.clear(sf::Color(18, 18, 22));
}
