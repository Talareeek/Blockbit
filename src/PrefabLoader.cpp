#include "../include/PrefabLoader.hpp"

#include <fstream>
#include <sstream>

#include <nlohmann/json.hpp>

#include "../include/AIComponent.hpp"
#include "../include/AssetManager.hpp"
#include "../include/ContentProfiles.hpp"
#include "../include/HealthComponent.hpp"
#include "../include/PhysicsComponent.hpp"
#include "../include/RenderComponent.hpp"
#include "../include/TransformComponent.hpp"

namespace PrefabLoader
{

namespace
{

using json = nlohmann::json;

std::string readFile(const std::filesystem::path& p)
{
    std::ifstream f(p);
    if (!f) return {};
    std::stringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

bool parsePrefab(const std::filesystem::path& file,
                 const std::string& ns,
                 LoadedPrefab& out,
                 std::string& err)
{
    const std::string raw = readFile(file);
    if (raw.empty()) { err = "cannot read " + file.string(); return false; }

    json j;
    try { j = json::parse(raw); }
    catch (const std::exception& e) { err = file.string() + ": " + e.what(); return false; }

    if (!j.contains("id") || !j["id"].is_string())
    { err = file.string() + ": missing 'id'"; return false; }

    out.key = j["id"].get<std::string>();
    if (!isValidRegistryKey(out.key))
    { err = file.string() + ": invalid key '" + out.key + "'"; return false; }

    if (!ns.empty())
    {
        const std::string prefix = ns + ":";
        if (out.key.compare(0, prefix.size(), prefix) != 0)
        { err = file.string() + ": key does not belong to namespace '" + ns + "'"; return false; }
    }

    Entity& e = out.data;

    // Transform — always present. Position is zero here; spawn helper writes
    // the real position.
    sf::Vector2<double> size{1.0, 1.0};
    if (j.contains("size") && j["size"].is_array() && j["size"].size() == 2)
    {
        size.x = j["size"][0].get<double>();
        size.y = j["size"][1].get<double>();
    }
    e.addComponent(TransformComponent{{0.0, 0.0}, size, sf::degrees(0.0f)});

    if (j.contains("render") && j["render"].is_object())
    {
        const auto& r = j["render"];
        const std::string tex_name = r.value("texture", std::string{});
        auto tex = ContentProfiles::textureFromName(tex_name);
        if (!tex.has_value())
        { err = file.string() + ": unknown texture '" + tex_name + "'"; return false; }

        sf::IntRect rect{{0, 0}, {16, 16}};
        if (r.contains("rect") && r["rect"].is_array() && r["rect"].size() == 4)
        {
            rect = sf::IntRect{{r["rect"][0].get<int>(), r["rect"][1].get<int>()},
                               {r["rect"][2].get<int>(), r["rect"][3].get<int>()}};
        }

        sf::Vector2f spriteSize{1.0f, 1.0f};
        if (r.contains("size") && r["size"].is_array() && r["size"].size() == 2)
        {
            spriteSize.x = r["size"][0].get<float>();
            spriteSize.y = r["size"][1].get<float>();
        }

        e.addComponent(RenderComponent{*tex, rect, spriteSize});
    }

    if (j.contains("physics") && j["physics"].is_object())
    {
        const auto& p = j["physics"];
        PhysicsComponent pc{
            {0.0, 0.0}, {0.0, 0.0}, {0.0, 0.0},
            p.value("mass", 1.0f),
            false,                                       // onGround — runtime
            p.value("is_solid",          true),
            p.value("is_static",         false),
            p.value("is_gravity_active", true)
        };
        e.addComponent(pc);
    }

    if (j.contains("health") && j["health"].is_object())
    {
        const auto& h = j["health"];
        const uint32_t hmax = h.value("max", 100u);
        HealthComponent hc;
        hc.health        = hmax;
        hc.maxHealth     = hmax;
        hc.baseMaxHealth = hmax;
        hc.damageable    = !h.value("invulnerable", false);
        e.addComponent(hc);
    }

    if (j.contains("ai") && j["ai"].is_object())
    {
        const auto& a = j["ai"];
        AIComponent ai;
        const std::string pers = a.value("personality", "passive");
        ai.personality = (pers == "aggressive") ? AIComponent::Personality::Aggressive
                                                : AIComponent::Personality::Passive;
        ai.detectionRange = a.value("detection_range", 8.0f);
        ai.attackRange    = a.value("attack_range",    1.5f);
        ai.fleeRange      = a.value("flee_range",      5.0f);
        ai.moveSpeed      = a.value("move_speed",      35.0f);
        e.addComponent(ai);
    }

    return true;
}

} // namespace

ScanResult scanPrefabs(const std::filesystem::path& packDir,
                       const std::string& packNamespace)
{
    ScanResult result;
    const auto dir = packDir / "prefabs";
    if (!std::filesystem::exists(dir)) return result;

    for (const auto& entry : std::filesystem::directory_iterator(dir))
    {
        if (!entry.is_regular_file()) continue;
        if (entry.path().extension() != ".json") continue;

        LoadedPrefab lp;
        if (!parsePrefab(entry.path(), packNamespace, lp, result.error))
        {
            result.ok = false;
            return result;
        }
        result.prefabs.push_back(std::move(lp));
    }
    return result;
}

} // namespace PrefabLoader
