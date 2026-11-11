#ifndef PREFAB_LOADER_HPP
#define PREFAB_LOADER_HPP

#include <filesystem>
#include <string>
#include <vector>

#include "Entity.hpp"
#include "Registry.hpp"

// Reads <pack>/prefabs/*.json using a flat schema:
//
//   {
//     "id": "ruby_mod:rubyling",
//     "size":    [1.0, 0.75],
//     "render":  {"texture": "pig", "rect": [0, 0, 16, 12], "size": [1.0, 0.75]},
//     "physics": {"mass": 1.0, "gravity": true, "collide_world": true,
//                 "collide_entities": false, "push_entities": true},
//     "health":  {"max": 100, "invulnerable": false},
//     "ai":      {"personality": "passive", "detection_range": 8.0,
//                 "attack_range": 1.5, "flee_range": 5.0, "move_speed": 35.0}
//   }
//
// All fields except "id" are optional; omitted fields drop the matching
// component. The resulting Entity has a zero UUID — callers clone it via
// cloneAs(generateUUID()) at spawn time.
namespace PrefabLoader
{

struct LoadedPrefab
{
    RegistryKey key;
    Entity      data{UUID{}};
};

struct ScanResult
{
    bool                      ok = true;
    std::string               error;
    std::vector<LoadedPrefab> prefabs;
};

// Scans `<packDir>/prefabs/` for *.json files. `packNamespace` is the pack's
// manifest id — every key must begin with "<packNamespace>:" or the file is
// rejected. Returns an error on the first malformed file; prefabs collected
// before that point are retained in `prefabs` for diagnostics.
ScanResult scanPrefabs(const std::filesystem::path& packDir,
                       const std::string&           packNamespace);

} // namespace PrefabLoader

#endif // PREFAB_LOADER_HPP
