# Helltaker PSP engine

This folder contains the source code of the engine that runs Helltaker PSP: a small game engine written from scratch in plain C for the PlayStation Portable.

It contains **no game assets and no game data**. Graphics, sound, music, dialogue and level layouts all come from the player's own copy of Helltaker and are not part of this repository.

## What it does

The engine reproduces Helltaker's gameplay and presentation on PSP hardware:

- Grid puzzle rules: moves, kicks, skeletons, boxes, spikes, keys and locks
- Dialogue system with portraits, cutscene animations, choices and endings
- A reimplementation of the Unity Animator: states, triggers, exit times, sprite timelines, position, scale, colour and rotation curves, and animation events
- Judgement's boss fight (the sin machine, chains, spikes and HP bar)
- Examtaker: laser raycasting, generators, the lab monitor and the Arch Mecha Demon boss timeline
- Pause menu, chapter select, floor pickers, door transitions and credits
- Streamed music and mixed sound effects
- Texture bundles loaded per scene to fit the PSP-1000's memory

## Files

| File | Purpose |
|---|---|
| `src/main.c` | Startup, input (with anti-mash filtering), save file, scripted test input |
| `src/gfx.c`, `src/gfx.h` | GU hardware renderer, texture bundles, sprites and text |
| `src/audio.c`, `src/audio.h` | Sound effect mixer and streamed music |
| `src/anim.c`, `src/anim.h` | Animator reimplementation |
| `src/scene.c` | Scene loading, timers, decorations, effects and camera shake |
| `src/puzzle.c` | Puzzle rules, death, restart and victory flow |
| `src/dialogue.c` | Dialogue and cutscene playback |
| `src/ui.c` | HUD, pause menu, chapter select, pickers, door transition, credits |
| `src/boss.c` | Judgement boss fight |
| `src/lab.c` | Examtaker lab floors: lasers, generators, monitor counter |
| `src/amd.c` | Examtaker final boss |
| `src/game.h` | Shared declarations |

## Building

The engine is built with the [pspdev](https://github.com/pspdev/pspdev) toolchain (PSPSDK) as a standard user-mode PSP application.

It includes a generated header, `gamedata.h`, and links against a generated `gamedata.c`. Those files hold the converted game data (sprite tables, animations, levels, dialogue) and are produced from a Helltaker installation by a separate converter, which is not published. The engine therefore does not build on its own from this repository.

## Notes

Helltaker and Examtaker were created by vanripper. All original names, characters, artwork, music and related assets belong to their respective rights holders. This engine is an unofficial fan reimplementation.
