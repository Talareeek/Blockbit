#include "../include/PrefabLoader.hpp"

#include <fstream>
#include <sstream>

#include <nlohmann/json.hpp>

#include "../include/AIComponent.hpp"
#include "../include/ContentProfiles.hpp"
#include "../include/EffectsComponent.hpp"
#include "../include/ExplosiveComponent.hpp"
#include "../include/HealthComponent.hpp"
#include "../include/InventoryComponent.hpp"
#include "../include/PhysicsComponent.hpp"
#include "../include/RenderComponent.hpp"
#include "../include/StaminaComponent.hpp"
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

// --- component builders ---------------------------------------------------
// Each builder receives the JSON object for its component slot (may be an
// empty object when the author wants the component with all defaults) and
// returns a concrete Component value installed onto the entity.

bool addTransform(Entity& e, const json& j)
{
    sf::Vector2<double> size{1.0, 1.0};
    if (j.contains("size") && j["size"].is_array() && j["size"].size() == 2)
    {
        size.x = j["size"][0].get<double>();
        size.y = j["size"][1].get<double>();
    }
    e.addComponent(TransformComponent{{0.0, 0.0}, size, sf::degrees(0.0f)});
    return true;
}

bool addRender(Entity& e, const json& j, std::string& err)
{
    const std::string tex_name = j.value("texture", std::string{});
    auto tex = ContentProfiles::textureFromName(tex_name);
    if (!tex.has_value()) { err = "unknown texture '" + tex_name + "'"; return false; }

    sf::IntRect rect{{0, 0}, {16, 16}};
    if (j.contains("rect") && j["rect"].is_array() && j["rect"].size() == 4)
    {
        rect = sf::IntRect{{j["rect"][0].get<int>(), j["rect"][1].get<int>()},
                           {j["rect"][2].get<int>(), j["rect"][3].get<int>()}};
    }

    sf::Vector2f sprite_size{1.0f, 1.0f};
    if (j.contains("size") && j["size"].is_array() && j["size"].size() == 2)
    {
        sprite_size.x = j["size"][0].get<float>();
        sprite_size.y = j["size"][1].get<float>();
    }
    e.addComponent(RenderComponent{*tex, rect, sprite_size});
    return true;
}

bool addPhysics(Entity& e, const json& j)
{
    e.addComponent(PhysicsComponent{
        {0.0, 0.0}, {0.0, 0.0}, {0.0, 0.0},
        j.value("mass", 1.0f),
        false,                                          // onGround — runtime
        j.value("is_solid",          true),
        j.value("is_static",         false),
        j.value("is_gravity_active", true)
    });
    return true;
}

bool addHealth(Entity& e, const json& j)
{
    const uint32_t hmax = j.value("max", 100u);
    HealthComponent hc;
    hc.health        = hmax;
    hc.maxHealth     = hmax;
    hc.baseMaxHealth = hmax;
    hc.damageable    = !j.value("invulnerable", false);
    e.addComponent(hc);
    return true;
}

bool addAI(Entity& e, const json& j)
{
    AIComponent ai;
    const std::string pers = j.value("personality", "passive");
    ai.personality = (pers == "aggressive") ? AIComponent::Personality::Aggressive
                                            : AIComponent::Personality::Passive;
    ai.detectionRange = j.value("detection_range", 8.0f);
    ai.attackRange    = j.value("attack_range",    1.5f);
    ai.fleeRange      = j.value("flee_range",      5.0f);
    ai.moveSpeed      = j.value("move_speed",      35.0f);
    e.addComponent(ai);
    return true;
}

bool addStamina(Entity& e, const json& j)
{
    const float max = j.value("max", StaminaComponent::DEFAULT_MAX);
    e.addComponent(StaminaComponent{max, max});
    return true;
}

bool addInventory(Entity& e, const json& j)
{
    const size_t slots = j.value("slots", static_cast<size_t>(36));
    e.addComponent(InventoryComponent{slots});
    return true;
}

bool addExplosive(Entity& e, const json& j)
{
    e.addComponent(ExplosiveComponent{
        j.value("force",           3.0f),
        j.value("destroy_blocks",  true),
        j.value("damage_entities", true),
        j.value("fuse_time",       3.0f),
        0.0f // timer — runtime
    });
    return true;
}

bool addEffects(Entity& e, const json& /*j*/)
{
    e.addComponent(EffectsComponent{});
    return true;
}

bool addComponentByName(Entity& e,
                        const std::string& name,
                        const json& j,
                        std::string& err)
{
    if (name == "transform") return addTransform(e, j);
    if (name == "render")    return addRender(e, j, err);
    if (name == "physics")   return addPhysics(e, j);
    if (name == "health")    return addHealth(e, j);
    if (name == "ai")        return addAI(e, j);
    if (name == "stamina")   return addStamina(e, j);
    if (name == "inventory") return addInventory(e, j);
    if (name == "explosive") return addExplosive(e, j);
    if (name == "effects")   return addEffects(e, j);
    err = "unknown component '" + name + "'";
    return false;
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

    if (!j.contains("components") || !j["components"].is_object())
    { err = file.string() + ": missing 'components' object"; return false; }

    for (const auto& [comp_name, comp_json] : j["components"].items())
    {
        std::string sub_err;
        if (!comp_json.is_object())
        {
            err = file.string() + ": component '" + comp_name + "' must be an object";
            return false;
        }
        if (!addComponentByName(out.data, comp_name, comp_json, sub_err))
        {
            err = file.string() + ": " + sub_err;
            return false;
        }
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
