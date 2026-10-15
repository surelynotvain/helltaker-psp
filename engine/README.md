# Helltaker PSP engine

This folder contains the source code that runs Helltaker PSP 2.1: plain C for the PlayStation Portable, written from scratch. It has two parts:

- `vcpe/src`: **Vain's C Portable Engine** (VCPE) 3.4, the shared PSP engine. It is also used by the Awaria PSP port.
- `src`: the Helltaker game code, built on top of VCPE.

It contains **no game assets and no game data**. Graphics, fonts, sound, music, dialogue and level layouts all come from the player's own copy of Helltaker and are not part of this repository.

## What it does

The engine reproduces Helltaker's gameplay and presentation on PSP hardware:

- Grid puzzle rules: moves, kicks, skeletons, boxes, spikes, keys and locks
- Dialogue system with portraits, cutscene animations, character spawns (Lucifer's guards, the Loremaster's maid), choices and endings
- A reimplementation of the Unity Animator: states, triggers, exit times, sprite timelines, position, scale, colour and rotation curves, and animation events
- Judgement's boss fight (the sin machine, chains, spikes and HP bar)
- Examtaker: laser raycasting, generators, the lab monitor and the Arch Mecha Demon boss timeline
- Pause menu, chapter select, floor pickers, door transitions and credits
- Text rasterized on the PSP from TrueType fonts, with kerning and the original shrink-to-fit rules
- Language packs (`LANG.PAK`): translated text, the start-up language choice and the pause menu switch
- Streamed music and mixed sound effects
- Texture bundles loaded per scene behind the loading screen, to fit the PSP-1000's memory

## Files

### Vain's C Portable Engine 3.4 (`vcpe/src`)

`vcpe.h` is the one header games include. It declares every VCPE module. This folder holds the source of the modules Helltaker links:

| File | Purpose |
|---|---|
| `vcpe_sys.c/.h` | HOME and sleep callbacks, CPU clock, paths next to the EBOOT, the error screen (`vcpe_fatal`), load steps for the boot log |
| `vcpe_pak.c/.h` | Data PAK reader that survives sleep/resume and slow memory sticks, header checks, chunked reads with progress |
| `vcpe_gfx.c/.h` | GU setup, frames, texture bundles and paletted swizzled pages, cached render state, quads, fills, strips, clipping, draw batching and CPU/GPU pipelining |
| `vcpe_audio.c/.h` | Software mixer: ADPCM sound effect voices and streamed ADPCM music, volume and fades |
| `vcpe_test.c/.h` | Scripted test input (`AUTOTEST.TXT`), snapshots and frame recording for repeatable runs |
| `vcpe_math.c/.h` | Table sine/cosine, fast atan2 and square root, a seedable random number generator |
| `vcpe_pad.c/.h` | Controller reading: button binds, the analog stick as a d-pad, press edges, anti-mash debounce |
| `vcpe_save.c/.h` | Save files with a backup copy, version, size and checksum |
| `vcpe_hud.c/.h` | Built-in 8x8 font, the performance overlay, PSP button icons |
| `vcpe_ttf.c/.h` | TrueType text on the PSP: glyph cache in textures, kerning, sub-pixel positions |
| `vcpe_mem.c/.h` | PSP-1000 (32 MB) detection, the 4 MB volatile memory partition, memory statistics |
| `vcpe_load.c/.h` | The loading screen: boot log, progress bar and animation |

The other headers in `vcpe/src` (3D, Lua, mouse and so on) are only declarations for modules Helltaker doesn't use.

#### The two engine pictures

Two generated headers are **not** in this repository, because they hold third-party pictures:

- `vcpe_loadimg.h`: the loading screen animation. Build it from any GIF with `vcpe/tools/mkloadimg.py GIF [SIZE] [STEP]` (defaults: 96 px, every 2nd frame).
- `vcpe_errimg.h`: the picture in the corner of the error screen. Build it from any image with `vcpe/tools/mkerrimg.py IMAGE [WIDTH]`.

Both tools need Python 3 with Pillow and write the header into `vcpe/src`.

### Helltaker (`src`)

| File | Purpose |
|---|---|
| `src/main.c` | Startup checks, input binds, save file, language choice, test commands |
| `src/gfx.c`, `src/gfx.h` | Sprites and text on top of the VCPE renderer, fonts, language packs |
| `src/audio.c`, `src/audio.h` | Helltaker's sound rules on the VCPE mixer (voices, looping effect, volume levels) |
| `src/anim.c`, `src/anim.h` | Animator reimplementation |
| `src/scene.c` | Scene loading (behind the loading screen), timers, decorations, effects and camera shake |
| `src/puzzle.c` | Puzzle rules, death, restart and victory flow |
| `src/dialogue.c` | Dialogue and cutscene playback, character spawns, inscriptions leading into dialogue |
| `src/ui.c` | HUD, pause menu, chapter select, pickers, door transition, credits |
| `src/boss.c` | Judgement boss fight |
| `src/lab.c` | Examtaker lab floors: lasers, generators, monitor counter |
| `src/amd.c` | Examtaker final boss |
| `src/game.h` | Shared declarations |

## Building

The engine is built with the [pspdev](https://github.com/pspdev/pspdev) toolchain (PSPSDK) as a standard user-mode PSP application. Compile the files in `src` and `vcpe/src` together, with both folders on the include path, after generating the two picture headers above.

The game code includes a generated header, `gamedata.h`, and links against a generated `gamedata.c`. Those files hold the converted game data (sprite tables, animations, levels, dialogue) and are produced from a Helltaker installation by a separate converter, which is not published. The engine therefore does not build on its own from this repository.

## Notes

Helltaker and Examtaker were created by vanripper. All original names, characters, artwork, music and related assets belong to their respective rights holders. This engine is an unofficial fan reimplementation.
