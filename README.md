# Helltaker PSP

A fan-made PSP port of **Helltaker** by SurelyNotVain, optimized for the PlayStation Portable and packaged for compatibility with **ARK-4** and **ARK-5**.

The current version contains the opening sequence, menus, Chapter 1 gameplay, dialogue, music, sound effects, failure sequences, and victory animations.

> [!IMPORTANT]
> This is an unofficial fan project. It is not affiliated with or endorsed by vanripper, Sony, the ARK developers, or the original Helltaker team.

## Features

- Native 480×272 PSP presentation
- Opening animation and cutscenes
- Playable Chapter 1
- D-pad and analog-stick controls
- Music and sound effects
- Chapter-selection screen
- Failure, dialogue, and victory sequences
- VBlank-synchronized double buffering
- Optimized background and animation rendering
- Streamed music with reduced memory usage
- PSP-1000-compatible memory configuration
- Proper HOME-button exit handling
- Support for Memory Stick and PSP Go internal storage
- ARK-4 and ARK-5 compatibility improvements

## Requirements

You will need:

- A PlayStation Portable capable of running unsigned homebrew
- ARK-4, ARK-5, or another compatible custom firmware
- Approximately 65 MB of free storage

Official firmware without a homebrew environment cannot launch this application.

## Download

Download the latest version from the **Releases** section of this GitHub repository.

Extract the downloaded archive before copying it to your PSP.

## Installation

The downloaded game directory must contain both files:

```text
HelltakerPSP/
├── EBOOT.PBP
└── MUSIC.PAK
```

Copy the complete `HelltakerPSP` directory to:

```text
ms0:/PSP/GAME/HelltakerPSP/
```

For PSP Go internal storage, use:

```text
ef0:/PSP/GAME/HelltakerPSP/
```

The final installation should look like this:

```text
ms0:/PSP/GAME/HelltakerPSP/EBOOT.PBP
ms0:/PSP/GAME/HelltakerPSP/MUSIC.PAK
```

After copying the files, open the PSP Game menu and launch **Helltaker PSP**.

> [!WARNING]
> Do not install `EBOOT.PBP` by itself. Music is streamed from `MUSIC.PAK`, which must remain in the same directory as the EBOOT.

## Updating

When installing a new version:

1. Exit the game completely.
2. Delete the previous `HelltakerPSP` directory from your PSP.
3. Extract the new release.
4. Copy the complete new `HelltakerPSP` directory to `PSP/GAME/`.
5. Confirm that both `EBOOT.PBP` and `MUSIC.PAK` were copied.

Replacing only the EBOOT may cause missing music or incompatibility if the music package format changes.

## Controls

| Button | Action |
|---|---|
| D-pad | Navigate menus and move |
| Analog stick | Move when fully pushed in a direction |
| Cross / X | Confirm, advance dialogue, or skip an animation |
| Circle / O | Go back or close a notification |
| Select | Open chapter selection from the main menu |
| R trigger | Restart the current level |
| HOME | Open the PSP exit menu |

Movement is turn-based. When using the analog stick, return it to the neutral position before making the next move.

## ARK compatibility

The application is packaged as a standard unsigned user-mode PSP application and does not require kernel-mode access.

Compatibility-related improvements include:

- Standard PSP homebrew PBP structure
- PSP-1000-compatible memory configuration
- No expanded-memory requirement
- Support for ARK-4 and ARK-5
- Support for `ms0:` and `ef0:` installation paths
- Music loading relative to the game directory
- Synchronized music streaming
- Proper HOME-button handling
- Safe audio and thread cleanup

The build is designed for:

| Environment | Compatibility |
|---|---|
| ARK-4 | Supported |
| ARK-5 | Supported |
| PSP-1000 | Supported |
| PSP-2000 | Supported |
| PSP-3000 | Supported |
| PSP Street | Supported |
| PSP Go Memory Stick | Supported |
| PSP Go internal storage | Supported through `ef0:` |

Hardware behavior may vary depending on the PSP model, storage device, ARK version, and installed plugins.

## Performance improvements

This release includes several optimizations intended for physical PSP hardware:

- Correct VBlank framebuffer swapping
- Double-buffered rendering to reduce tearing and flickering
- Cached static backgrounds and animation frames
- Faster background drawing
- Fewer expensive per-pixel calculations
- Removed redundant full-screen clears
- Reduced framebuffer redraw work
- Lower audio resource usage
- Small streamed-music buffer
- Correct music playback speed
- Safer sound mixing
- Fixed audio streaming races
- Fixed animation timing problems
- Fixed out-of-bounds animation access
- Improved shutdown and resource cleanup

## Troubleshooting

### The game does not appear in the PSP Game menu

Confirm that the directory structure is exactly:

```text
PSP/GAME/HelltakerPSP/EBOOT.PBP
```

Avoid adding an extra nested directory such as:

```text
PSP/GAME/HelltakerPSP/HelltakerPSP/EBOOT.PBP
```

### The game starts without music

Make sure this file exists:

```text
PSP/GAME/HelltakerPSP/MUSIC.PAK
```

The filename must remain uppercase and must be in the same directory as `EBOOT.PBP`.

Do not rename or move `MUSIC.PAK`.

### The game displays a black screen or returns to the XMB

Try the following:

1. Confirm that both release files were copied completely.
2. Update ARK to a recent version.
3. Launch the game directly from the XMB.
4. Restart the PSP and try again.
5. Temporarily disable game-related plugins to check for conflicts.
6. Recopy the release files in case they were corrupted.
7. Try another Memory Stick if available.

### Music pauses or skips

Slow or damaged storage can interrupt streamed audio.

Try:

- Copying the game again
- Using another Memory Stick
- Closing unnecessary plugins
- Testing the game from PSP Go internal storage
- Confirming that `MUSIC.PAK` was copied completely

### PSP Go installation problems

Try both supported storage locations:

```text
ms0:/PSP/GAME/HelltakerPSP/
```

```text
ef0:/PSP/GAME/HelltakerPSP/
```

Make sure the selected storage device is visible to your installed ARK version.

### Controls do not respond correctly

Restart the game and ensure that no controller-related plugins are active.

For analog movement, fully push the analog stick in one direction and return it to the center before attempting the next move.

### The game freezes when exiting

Use the PSP HOME menu and select the normal exit option.

If the problem continues, include your PSP model, ARK version, and active plugins when submitting a bug report.

## Reporting bugs

Bug reports and hardware compatibility reports are welcome.

When opening an issue, please include:

- PSP model
- System software version
- ARK version
- Installation path (`ms0:` or `ef0:`)
- Whether the game was launched from the XMB or another launcher
- Installed plugins that may affect games, controls, or audio
- Steps needed to reproduce the problem
- Whether music was playing when the problem occurred
- A photograph or video if the problem is visual

Example report:

```text
PSP model: PSP-3000
System software: 6.61
ARK version: ARK-5
Installation: ms0:/PSP/GAME/HelltakerPSP/
Launch method: XMB

Problem:
The game returns to the XMB after selecting New Game.

Steps:
1. Launch the game.
2. Skip the opening animation.
3. Select New Game.
4. Press X.
```

## Known limitations

- The current version focuses on Chapter 1.
- Later chapters shown in chapter selection are not yet playable.
- `MUSIC.PAK` is required and cannot be removed.
- Compatibility may be affected by third-party PSP plugins.
- Performance may vary with slower Memory Sticks.
- Additional testing on different PSP models and ARK versions is welcome.

## Feedback

Feedback, testing results, and bug reports are welcome through GitHub Issues.

The source code and development tools are not included in the public release.

Please do not redistribute modified releases using the same name without clearly identifying that they are unofficial modifications.

## Credits

- **Helltaker** was created by **vanripper**.
- PSP homebrew development is supported by the PSPSDK and pspdev communities.
- ARK compatibility is provided by the ARK community and its contributors.
- This PSP adaptation is an unofficial fan project.

All original Helltaker names, characters, artwork, music, and related assets belong to their respective creators and rights holders.

## Disclaimer

This project is provided for educational, preservation, and entertainment purposes.

It is not an official release and is provided without any warranty. The developers of this port are not responsible for data loss, storage corruption, console damage, or problems caused by unsupported firmware modifications or third-party plugins.

Users are responsible for complying with any licenses and distribution terms that apply to the original game and its assets.
