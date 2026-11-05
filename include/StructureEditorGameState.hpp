#ifndef STRUCTURE_EDITOR_GAME_STATE_HPP
#define STRUCTURE_EDITOR_GAME_STATE_HPP

#include "GameState.hpp"
#include "StructureTemplate.hpp"

#include <SFML/Graphics/RenderTexture.hpp>
#include <SFML/Graphics/Texture.hpp>

#include <string>
#include <vector>

// ImGui-driven structure editor. Lets the user paint a StructureTemplate on a
// 2D grid (same semantics as the world — bottom-up rows, side-view), set the
// anchor, drop block entities and entity spawns, and save/load BBT files from
// resources/structures/. Rendered using the same block atlas the game uses in
// world chunks, so the preview matches in-world placement visually.
class StructureEditorGameState : public GameState
{
public:

    enum class Tool : int
    {
        Paint         = 0, // overwrite target cell with selected block + mode
        Erase         = 1, // reset target cell to {Air, Skip}
        Pick          = 2, // read the target cell back into the palette
        FillRect      = 3, // drag a rectangle, flood it on release
        SetAnchor     = 4, // move the anchor marker to the clicked cell
        AddBlockEntity= 5, // attach a BlockEntity to the clicked cell
        AddSpawn      = 6, // attach a PendingSpawn to the clicked cell
        RemoveMarker  = 7, // remove any block entity / spawn on the clicked cell
    };

    StructureEditorGameState(Game* game);

    void handleEvent(const sf::Event& event) override;
    void update(float dt) override;
    void render(sf::RenderWindow& window) override;

private:

    // ---- template state -----------------------------------------------------

    StructureTemplate tmpl;
    std::string       current_name;   // filename stem (empty = unsaved)
    bool              dirty = false;  // unsaved changes

    // ---- palette / tool state ----------------------------------------------

    int                     palette_index = 0; // index into BLOCK_OPTIONS
    StructurePlacementMode  paint_mode    = StructurePlacementMode::Overwrite;
    Tool                    tool          = Tool::Paint;

    // Block entity / spawn pickers for AddBlockEntity / AddSpawn tools.
    int                     block_entity_kind_index = 0; // into BE_OPTIONS
    int                     spawn_prefab_index      = 0; // into SPAWN_OPTIONS

    // ---- canvas viewport ---------------------------------------------------

    float                 cell_pixels = 24.0f;
    float                 pan_x = 0.0f;
    float                 pan_y = 0.0f;
    sf::RenderTexture     canvas;
    sf::Vector2u          canvas_size{640u, 640u};
    sf::Vector2f          canvas_display_size{640.0f, 640.0f};

    // Hover + drag tracking in template-local cell coordinates. ty_* use the
    // template's Y convention (0 at bottom).
    bool                  has_hover = false;
    int                   hover_tx  = 0;
    int                   hover_ty  = 0;

    bool                  dragging = false;        // left mouse held
    int                   drag_start_tx = 0;
    int                   drag_start_ty = 0;

    // ---- file browser ------------------------------------------------------

    char                        name_buffer[64]        = "";
    std::vector<std::string>    saved_files;
    int                         selected_saved_index   = -1;

    // ---- status / dialogs --------------------------------------------------

    std::string           last_status;
    bool                  show_confirm_discard = false;
    bool                  should_close         = false;

    // Pending action for the discard confirmation dialog.
    enum class PendingAction { None, LeaveEditor, NewTemplate, LoadSelected };
    PendingAction         pending = PendingAction::None;

    // ---- helpers -----------------------------------------------------------

    void resetToBlank(int w, int h);
    void resizeTemplate(int new_w, int new_h);
    bool inBounds(int tx, int ty) const;

    void refreshCanvas();

    // ImGui panels
    void drawLeftPanel();
    void drawRightPanel();
    void drawCanvasPanel();
    void drawStatusBar();
    void drawDiscardConfirmation();

    // File ops
    void scanSavedFiles();
    void saveCurrent();
    void loadFile(const std::string& stem);

    // Canvas interaction
    void handleCanvasClick(int tx, int ty, bool shift_down);
    void handleFillRelease(int ax, int ay, int bx, int by);

    // Draw a single cell (block texture + mode tint + markers) into the canvas
    // render texture. tx/ty are template-local.
    void drawCellIntoCanvas(sf::RenderTexture& rt, int tx, int ty) const;
};

#endif // STRUCTURE_EDITOR_GAME_STATE_HPP
