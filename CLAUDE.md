# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build / run

The project is C++20 + SFML 3.x, built with CMake + Ninja. Build type defaults to Debug.

```bash
./build.sh                    # debug build (clang/clang++ by default)
./build.sh release            # release build
./build.sh rebuild            # wipe build/ and rebuild
./build.sh run                # build + launch main menu
./build.sh run-world <name>   # build + launch straight into a world
./run.sh [world]              # same, thinner wrapper
```

Binary lands at `build/bin/Blockbit`. Worlds are stored in `~/Blockbit/saves/<name>/` (Linux) or `%APPDATA%/Blockbit/saves/<name>/` (Windows). CLI args: `--load <world> <nick>`, `--host <world> <nick>`, `--join <host[:port]> <nick>` — nickname is a positional arg immediately after the world/address.

**Reconfigure after adding or removing `.cpp` files.** `CMakeLists.txt` uses `file(GLOB_RECURSE SOURCES "src/*.cpp")`, so new/removed sources are only picked up on reconfigure:

```bash
cd build && cmake -G Ninja -DCMAKE_BUILD_TYPE=Debug .. && cd ..
```

**Compile one file for a fast error check** (much faster than full build):

```bash
ninja -C build CMakeFiles/Blockbit.dir/src/<File>.cpp.o
```

There is no test suite. Verification is manual via `./run.sh`. A separate chunk-dump utility lives at `tools/inspector/` (built with its own `build.sh`).

Dependencies: SFML (Graphics/Window/System/Audio), zstd, tinyxml2 come from the system. Dear ImGui + imgui-sfml and nlohmann/json are fetched by CMake. `asio` is used standalone (`ASIO_STANDALONE` compile def) — do not add Boost.

## Architecture

### Single-player and multi-player share one code path

Even `--load` runs a `GameServer` in-process. The client talks to it through a `LoopbackServerTransport`/`LoopbackClientTransport` pair instead of UDP. When something feels like it "should be local," check whether it's actually crossing the client/server boundary — in this codebase it almost always is.

- `GameServer::update` in `src/GameServer.cpp` is the server tick: `syncConnections → processIncoming → processWorldInputs → systems (AI, Explosive, Health, Physics, PlayerControlled, Inventory, ChunkLoad, ChunkUnload, Daycycle, Animation, Transform) → world.tick → dispatchPendingViewOpens → updateContentViews → streamChunksToClients → broadcastBlockUpdates → broadcastSnapshot`.
- `ClientGameState` holds the local `ClientWorld`, a `ClientTransport`, and (for host/singleplayer) an in-process `GameServer`. It drains inputs each frame and ships them as `InputPacket`s.
- Transports are abstract (`ServerTransport`, `ClientTransport`). Concrete: `NetworkServerTransport`/`NetworkClientTransport` (asio-based), `LoopbackServerTransport`/`LoopbackClientTransport`, and `CompositeServerTransport` for listen-server (loopback + network simultaneously).

### ECS + delta-sync snapshots

Entities (`include/Entity.hpp`) are `UUID → map<type_index, unique_ptr<Component>>`. Components derive from `Component` (`include/Component.hpp`) and are self-serializing via the custom BBT tag format (`include/BBT.hpp`, used for both save files and wire packets).

Three important fields on every `Component`:
- `dirty` — set to `true` after you mutate a component so `Entity::serializeDirtyForClient()` picks it up for the next `SnapshotPacket`. **Forgetting to set `dirty = true` after mutating a component is the single most common source of "the change doesn't show up on the client" bugs.** `ServerWorld` sends only dirty component tags; `clearDirty()` runs after broadcast.
- `side()` — `Server` means the component is never serialized to clients (e.g. `PlayerControlledComponent` is `Both` because the server tells the client the player's view state).
- `clone()` — required because `Entity::cloneAs` deep-copies components.

Systems are plain functions (not classes) that take `World&` and sometimes `dt`. See `src/PlayerControlledSystem.cpp` for the canonical shape: iterate `world.getEntities()`, check `hasComponent<...>`, mutate, flip `dirty`.

### Player action flow

A player click → use/place is a round-trip, not a local action:

1. `Input.cpp::getInputs` reads SFML state into `Input` structs each client frame.
2. Client appends them to its `inputs` buffer and sends `InputPacket` to the server.
3. `GameServer::processIncoming` queues inputs per client. On the next tick, `processWorldInputs` applies them to the player's `PlayerControlledComponent` (sets `mid_usage`, `mid_attack`, `cursor_position`, `drops`, etc.).
4. `PlayerControlledSystem` (next in the pipeline) consumes those flags, enforces a `0.05f` (1/20 s) cooldown via `since_last_use`, performs the block placement / `itemDatabase[...].onUse` / chest open, flips `dirty = true`.
5. The resulting state change is picked up by `broadcastSnapshot` / `broadcastBlockUpdates`.

If you add a new player-driven action, the pattern is: new `InputType` → packet (de)serialization in `src/Packet.cpp` → flag on `PlayerControlledComponent` → logic in `PlayerControlledSystem` (behind the shared use/attack cooldown if applicable).

### Server-authoritative UI "views" (chest, inventory)

Any interactable surface that opens a UI panel (chests, inventories) uses the view protocol in `include/View.hpp`:

- `ServerView` lives on the server (owned per-client in `ClientData::content_view`). It drives world mutations and emits `SetContentData` payloads.
- `ClientView` is a UI element on the client (owned by `ClientGameState::content_view`). It consumes `SetContentData` and emits `ViewEventData` payloads back.
- To open a view, push a `std::unique_ptr<ServerView>` onto `PlayerControlledComponent::pending_view_opens`. `GameServer::dispatchPendingViewOpens` sends the `OpenView` packet; `updateContentViews` ticks the active view and flushes queued `SetContent` deltas.
- Payload byte layout is private to each concrete view type — both ends must agree. Follow `ServerChestView` / `ClientChestView` for the template.

### World / chunks

- `Chunk` is a fixed 16 × 256 block grid plus climate/biome strips. `CHUNK_WIDTH`, `CHUNK_HEIGHT` live in `include/Chunk.hpp`.
- `World` holds `unordered_map<int, Chunk>` keyed by chunk column. `World::SIMULATION_DISTANCE = 10`, soft cap `PREFFERED_CHUNKS_LOADED = 16`, hard cap `MAX_CHUNKS_LOADED = 24`.
- Blocks are `{BlockID id, uint8_t metadata}`. Metadata is overloaded per block (e.g. water level, see `include/Block.hpp`). Block behaviour (hardness, drops, mining tool, render bounds, post-place update hook) is table-driven via `blockDatabase` in `src/Block.cpp`.
- Fluids tick every `FLUID_TICK = 0.5f` seconds via `updateFluids(world)`.
- Server broadcasts individual block changes through `world.pending_block_updates`; after a `setBlock` the position must land there for clients to see it. `ServerWorld::setBlock` handles this.

### Items

`itemDatabase` in `src/Item.cpp` maps `ItemID` → `ItemData` (texture, stack size, optional `ToolProperties`, `onUse` lambda). Items that place a block go through `PlayerControlledSystem`'s block-placement branch (keyed on `ItemCategory::Block`); non-block items go through their `onUse(World&, mouse, user) -> bool`. Returning `true` from `onUse` consumes one from the stack.

Entity collisions block solid placement but let water/fire overlap — this is encoded in the block-placement branch of `PlayerControlledSystem`, not inside `setBlock`.

## Gotchas worth remembering

- **Don't amend commits** unless explicitly asked. Create new commits instead.
- **Mutations without `dirty = true` are silent on the client.** When touching any component, flip the flag.
- **`src/*.cpp` is globbed.** Reconfigure CMake when you add or delete source files.
- **Default compilers are clang/clang++** (set by `build.sh`). If the user wants gcc, override with `CC=gcc CXX=g++ ./build.sh`.
- **Debug symbols are kept in Release** (`-O3 -g`), so release builds are still gdb-able.
- **ImGui state** persists in `imgui.ini` (gitignored); don't commit it.
- **The `include/` and `src/` split is strict** — headers are included as `"../include/Foo.hpp"` in many `.cpp` files; keep that pattern or everything in `src/` for new files.
