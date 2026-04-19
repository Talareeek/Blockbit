---
name: run
description: Build and run Blockbit with the GUI for manual testing. Use when the user needs to manually interact with the game to verify something (e.g. click through UI, observe behavior in-game). **IMPORTANT: Automatically run this after any UI, rendering, or graphics changes so the user can see the visual effect.** After the game launches, wait for the user to report what they observe.
allowed-tools: shell
---

# Run Blockbit for Manual Testing

Build the project and launch it so the user can manually test something in the game's GUI.

## When to use this skill

- After any changes to UI elements (buttons, text, panels, menus)
- After rendering or graphics changes
- After layout or positioning modifications
- When the user asks to test or verify something visually
- **Automatically run after UI/graphics fixes** so the user can see the results immediately

## Steps

1. Build the project:

```bash
ninja -C build
```

2. Launch the game:

```bash
./run.sh
```

3. Tell the user the game is running and ask them to test what they need. Wait for their feedback before proceeding.

## If the build needs CMake reconfiguration

```bash
mkdir -p build && cd build && cmake -G Ninja -DCMAKE_BUILD_TYPE=Debug .. && cd .. && ninja -C build
```
