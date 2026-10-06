# Helltaker PSP engine

This folder contains the source code that runs Helltaker PSP: plain C for the PlayStation Portable, written from scratch. It has two parts:

- `vcpe/src`: **Vain's C Portable Engine** (VCPE), the shared PSP engine base. It is also used by the Awaria PSP port.
- `src`: the Helltaker game code, built on top of VCPE.

It contains **no game assets and no game data**. Graphics, sound, music, dialogue and level layouts all come from the player's own copy of Helltaker and are not part of this repository.

## What it does

The engine reproduces Helltaker's gameplay and presentation on PSP hardware:

- Grid puzzle rules: moves, kicks, skeletons, boxes, spikes, keys and locks
- Dialogue system with portraits, cutscene animations, choices and endings
- A reimplementation of the Unity Animator: states, triggers, exit times, sprite timelines, position, scale, colour and rotation curves, and animation events
- Judgement's boss fight (the sin machine, chains, spikes and HP bar)
- Examtaker: laser raycasting, generators, the lab monitor and the Arch Mecha Demon boss timeline
- Pause menu, chapter select, floor pickers, door transitions and credits
- Language packs (`LANG.PAK`): pictures of translated texts, the start-up language choice and the pause menu switch
- Streamed music and mixed sound effects
- Texture bundles loaded per scene to fit the PSP-1000's memory

## Files

### Vain's C Portable Engine (`vcpe/src`)

| File | Purpose |
|---|---|
| `vcpe.h` | The one header games include |
| `vcpe_sys.c`, `vcpe_sys.h` | HOME and sleep callbacks, CPU clock, paths next to the EBOOT |
| `vcpe_pak.c`, `vcpe_pak.h` | Data PAK reader that survives sleep/resume and slow memory sticks |
| `vcpe_gfx.c`, `vcpe_gfx.h` | GU setup, frames, texture bundles and paletted swizzled pages, cached render state, quads, fills, strips, clipping |
| `vcpe_audio.c`, `vcpe_audio.h` | Software mixer: ADPCM sound effect voices and streamed ADPCM music, volume and fades |
| `vcpe_test.c`, `vcpe_test.h` | Scripted test input (`AUTOTEST.TXT`) and frame recording for repeatable runs |

### Helltaker (`src`)

| File | Purpose |
|---|---|
| `src/main.c` | Startup, input (with anti-mash filtering), save file, language choice, test commands |
| `src/gfx.c`, `src/gfx.h` | Sprites and text on top of the VCPE renderer, language packs |
| `src/audio.c`, `src/audio.h` | Helltaker's sound rules on the VCPE mixer (voices, looping effect, volume levels) |
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

The engine is built with the [pspdev](https://github.com/pspdev/pspdev) toolchain (PSPSDK) as a standard user-mode PSP application. Compile the files in `src` and `vcpe/src` together, with both folders on the include path.

The game code includes a generated header, `gamedata.h`, and links against a generated `gamedata.c`. Those files hold the converted game data (sprite tables, animations, levels, dialogue) and are produced from a Helltaker installation by a separate converter, which is not published. The engine therefore does not build on its own from this repository.

## Notes

Helltaker and Examtaker were created by vanripper. All original names, characters, artwork, music and related assets belong to their respective rights holders. This engine is an unofficial fan reimplementation.
