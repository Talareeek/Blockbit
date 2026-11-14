// One-off generator that produces the mineshaft jigsaw corridor set:
//   * reads  resources/packs/blockbit/structures/mineshaft.bbt
//   * stamps a Left + Right jigsaw onto it (connector "mineshaft_h",
//     pool "blockbit:mineshaft_corridors") and writes it back
//   * generates three corridor pieces (flat, step-up, step-down) with their
//     own paired jigsaws so chain pieces can rise or drop one row between
//     segments
//   * writes <structures>/{mineshaft_corridor_flat,up,down}.{bbt,json}
//   * writes <jigsaw_pools>/mineshaft_corridors.json
//   * rewrites <structures>/mineshaft.json with max_chain_depth=10
//
// Not part of normal gameplay — run once after the structure feature changes:
//   ninja -C build structure_gen && build/bin/structure_gen
//
// After that, the files live in the pack and worldgen picks them up on the
// next world-load.

#include "../../include/StructureTemplate.hpp"
#include "../../include/BBT.hpp"
#include "../../include/Block.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <vector>

namespace fs = std::filesystem;

static const fs::path PACK_DIR = "resources/packs/blockbit";
static const fs::path STRUCT_DIR = PACK_DIR / "structures";
static const fs::path POOL_DIR   = PACK_DIR / "jigsaw_pools";

// All corridor pieces share these dimensions so the chain fits together
// cleanly in both X and Y. If you bump these, remember to keep the two
// opposite jigsaws at compatible Y offsets within each piece.
static constexpr int CORRIDOR_W = 7;
static constexpr int CORRIDOR_H = 4;

StructureTemplate loadTemplate(const fs::path& p)
{
    std::ifstream in(p, std::ios::binary);
    if (!in) throw std::runtime_error("cannot open " + p.string());
    std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(in)),
                                std::istreambuf_iterator<char>());
    BBT bbt = BBT::load(bytes);
    StructureTemplate t;
    t.deserialize(bbt.root().at("template"));
    return t;
}

void saveTemplate(const fs::path& p, const StructureTemplate& t, const std::string& name)
{
    TagCompound root;
    root["name"]     = Tag(name);
    root["template"] = t.serialize();
    BBT bbt("structure", root);
    const auto bytes = bbt.save();

    std::ofstream out(p, std::ios::binary | std::ios::trunc);
    out.write(reinterpret_cast<const char*>(bytes.data()),
              static_cast<std::streamsize>(bytes.size()));
}

StructureCell wall() { return StructureCell{Block{BlockID::Cobblestone, 0}, StructurePlacementMode::Overwrite, -1, 255}; }
StructureCell plank(){ return StructureCell{Block{BlockID::Oak_Planks,  0}, StructurePlacementMode::Overwrite, -1, 255}; }
StructureCell air()  { return StructureCell{Block{BlockID::Air,         0}, StructurePlacementMode::Overwrite, -1, 255}; }
StructureCell skip() { return StructureCell{Block{BlockID::Air,         0}, StructurePlacementMode::Skip,      -1, 255}; }

// Build a corridor-shaped template with air inside, floor/ceiling of planks,
// and left/right jigsaw entry/exit rows at `left_y` and `right_y`.
// Uses a small palette on the walls so adjacent segments visually vary.
StructureTemplate makeCorridor(int left_y, int right_y)
{
    StructureTemplate t;
    t.width  = CORRIDOR_W;
    t.height = CORRIDOR_H;
    t.anchor_x = 0;
    t.anchor_y = left_y;

    // Wall palette: cobblestone 70% / stone 20% / iron ore 10% for a rough
    // "unfinished mine" look that varies between adjacent segments.
    StructurePalette wall_pool;
    wall_pool.blocks  = {Block{BlockID::Cobblestone, 0}, Block{BlockID::Stone, 0}, Block{BlockID::Iron_Ore, 0}};
    wall_pool.weights = {70, 20, 10};
    t.palettes.push_back(wall_pool);

    t.cells.resize(static_cast<std::size_t>(CORRIDOR_W * CORRIDOR_H), skip());

    auto set = [&](int x, int y, StructureCell c)
    {
        t.cells[y * CORRIDOR_W + x] = c;
    };
    auto set_pool_wall = [&](int x, int y)
    {
        StructureCell c = wall();
        c.palette = 0;
        set(x, y, c);
    };

    // Carve the inside — 2 rows of air between floor and ceiling.
    // Row layout: 0 = floor, 1..CORRIDOR_H-2 = air, CORRIDOR_H-1 = ceiling.
    for (int x = 0; x < CORRIDOR_W; x++)
    {
        set_pool_wall(x, 0);                 // floor (random variants)
        set_pool_wall(x, CORRIDOR_H - 1);    // ceiling
        for (int y = 1; y < CORRIDOR_H - 1; y++)
            set(x, y, air());
    }

    // Occasional support beam — ~1 in 4 columns has an oak plank on the
    // floor + ceiling, giving the eye something to latch onto so chained
    // segments don't feel like one endless tube. Probability knob is a nice
    // showcase of the per-cell probability feature too.
    for (int x = 1; x < CORRIDOR_W - 1; x += 2)
    {
        StructureCell beam = plank();
        beam.probability = 96; // ~37% chance per candidate column
        set(x, 1, beam);
        set(x, CORRIDOR_H - 2, beam);
    }

    // The two jigsaws live at the "entry row" of each side. Blocks at those
    // spots stay Skip so neighbour terrain shows through until the next
    // segment stamps over them.
    StructureJigsaw jl;
    jl.rx = 0;
    jl.ry = left_y;
    jl.facing = StructureJigsawFacing::Left;
    jl.connector = "mineshaft_h";
    jl.pool      = "blockbit:mineshaft_corridors";
    t.jigsaws.push_back(jl);

    StructureJigsaw jr;
    jr.rx = CORRIDOR_W - 1;
    jr.ry = right_y;
    jr.facing = StructureJigsawFacing::Right;
    jr.connector = "mineshaft_h";
    jr.pool      = "blockbit:mineshaft_corridors";
    t.jigsaws.push_back(jr);

    return t;
}

void writeJson(const fs::path& p, const std::string& contents)
{
    std::ofstream out(p);
    out << contents;
}

int main()
{
    try
    {
        fs::create_directories(STRUCT_DIR);
        fs::create_directories(POOL_DIR);

        // ---- 1. Mineshaft: add paired jigsaws ------------------------------
        //
        // The existing mineshaft.bbt was authored small (17 columns wide per
        // the earlier hex dump). Drop a Left + Right jigsaw at the middle
        // row of the "entrance" strip so corridors fan out both ways and the
        // MirrorX variant still has something to attach to.
        {
            const fs::path mineshaft_bbt = STRUCT_DIR / "mineshaft.bbt";
            StructureTemplate t = loadTemplate(mineshaft_bbt);
            std::cout << "[gen] mineshaft loaded: " << t.width << " x " << t.height
                      << ", anchor=(" << t.anchor_x << "," << t.anchor_y << ")"
                      << ", existing jigsaws=" << t.jigsaws.size() << "\n";

            // Clear any previous jigsaws so repeat runs stay idempotent.
            t.jigsaws.clear();

            // Attach jigsaws two rows above the anchor floor so chain pieces
            // link at corridor-mid-height, not at the base rails. Clamp to
            // bounds just in case the template is shorter than expected.
            const int link_y = std::clamp(t.anchor_y + 1, 0, t.height - 2);

            StructureJigsaw jl;
            jl.rx = 0;
            jl.ry = link_y;
            jl.facing = StructureJigsawFacing::Left;
            jl.connector = "mineshaft_h";
            jl.pool      = "blockbit:mineshaft_corridors";
            t.jigsaws.push_back(jl);

            StructureJigsaw jr;
            jr.rx = t.width - 1;
            jr.ry = link_y;
            jr.facing = StructureJigsawFacing::Right;
            jr.connector = "mineshaft_h";
            jr.pool      = "blockbit:mineshaft_corridors";
            t.jigsaws.push_back(jr);

            saveTemplate(mineshaft_bbt, t, "mineshaft");
            std::cout << "[gen] mineshaft.bbt rewritten with 2 jigsaws at ry=" << link_y << "\n";
        }

        // ---- 2. Three corridor segments -----------------------------------
        //
        // Flat: both ends at middle row — most common roll.
        // StepUp: right end sits 1 row higher than the left — chain rises.
        // StepDown: right end sits 1 row lower than the left — chain drops.
        //
        // With a mix of weights in the pool, long chains get a gently
        // varying altitude instead of a perfectly flat tunnel.
        const int mid = CORRIDOR_H / 2 - 1; // row 1 in a 4-tall corridor
        {
            StructureTemplate flat = makeCorridor(mid, mid);
            saveTemplate(STRUCT_DIR / "mineshaft_corridor_flat.bbt", flat,
                         "mineshaft_corridor_flat");

            StructureTemplate up   = makeCorridor(mid, mid + 1);
            saveTemplate(STRUCT_DIR / "mineshaft_corridor_up.bbt", up,
                         "mineshaft_corridor_up");

            StructureTemplate down = makeCorridor(mid, mid - 1 < 0 ? 0 : mid - 1);
            saveTemplate(STRUCT_DIR / "mineshaft_corridor_down.bbt", down,
                         "mineshaft_corridor_down");

            std::cout << "[gen] wrote 3 corridor templates (flat/up/down)\n";
        }

        // ---- 3. Structure JSON for each new corridor ----------------------
        //
        // spawn_chance=0 so these never seed on their own — only through the
        // jigsaw pool. grid still has to be positive; placement scan short-
        // circuits on spawn_chance anyway.
        const char* corridor_names[] = {
            "mineshaft_corridor_flat",
            "mineshaft_corridor_up",
            "mineshaft_corridor_down",
        };
        for (const char* name : corridor_names)
        {
            std::string j;
            j += "{\n";
            j += "    \"id\": \"blockbit:";  j += name; j += "\",\n";
            j += "    \"template\": \"";     j += name; j += ".bbt\",\n";
            j += "    \"rule\": {\n";
            j += "        \"grid\": 1,\n";
            j += "        \"spawn_chance\": 0.0,\n";
            j += "        \"anchor_mode\": \"below_surface\",\n";
            j += "        \"allow_mirror\": false,\n";
            j += "        \"max_chain_depth\": 0\n";
            j += "    }\n";
            j += "}\n";
            writeJson(STRUCT_DIR / (std::string(name) + ".json"), j);
        }
        std::cout << "[gen] wrote 3 corridor JSONs\n";

        // ---- 4. Pool JSON --------------------------------------------------
        {
            std::string j =
                "{\n"
                "    \"id\": \"blockbit:mineshaft_corridors\",\n"
                "    \"entries\": [\n"
                "        {\"structure\": \"blockbit:mineshaft_corridor_flat\", \"weight\": 6},\n"
                "        {\"structure\": \"blockbit:mineshaft_corridor_up\",   \"weight\": 2},\n"
                "        {\"structure\": \"blockbit:mineshaft_corridor_down\", \"weight\": 2}\n"
                "    ]\n"
                "}\n";
            writeJson(POOL_DIR / "mineshaft_corridors.json", j);
            std::cout << "[gen] wrote jigsaw pool JSON\n";
        }

        // ---- 5. Update mineshaft.json to enable chaining ------------------
        //
        // Keep the existing rule values but lift max_chain_depth so the root
        // extends up to 10 corridor segments either side (20 total). The
        // generator caps chain reach at 10*max_template_dim blocks of
        // neighbour-chunk search, which is fine for this template size.
        {
            // Each root reaches ~max_chain_depth * corridor_width blocks per
            // side (chain is bidirectional). With depth=8 and 7-wide pieces
            // that's ~56 blocks either way, so ~130-block footprint + 17 for
            // the mineshaft itself. Spacing grid at ~1024 blocks with a
            // modest spawn_chance keeps clusters comfortably far apart.
            std::string j =
                "{\n"
                "    \"id\": \"blockbit:mineshaft\",\n"
                "    \"template\": \"mineshaft.bbt\",\n"
                "    \"rule\":\n"
                "    {\n"
                "        \"grid\": 1024,\n"
                "        \"spawn_chance\": 0.5,\n"
                "        \"anchor_mode\": \"below_surface\",\n"
                "        \"min_surface_y\": 0,\n"
                "        \"max_surface_y\": 255,\n"
                "        \"allow_mirror\": true,\n"
                "        \"max_chain_depth\": 8\n"
                "    }\n"
                "}\n";
            writeJson(STRUCT_DIR / "mineshaft.json", j);
            std::cout << "[gen] mineshaft.json updated with grid=1024 spawn=0.5 chain=8\n";
        }

        std::cout << "[gen] done.\n";
        return 0;
    }
    catch (const std::exception& e)
    {
        std::cerr << "[gen] error: " << e.what() << "\n";
        return 1;
    }
}
