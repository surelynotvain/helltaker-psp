# Translating the Helltaker PSP port

The PSP port shows every text as a pre-rendered picture. A translation is one file, `LANG.PAK`,
with pictures of the translated texts. If `LANG.PAK` sits next to `EBOOT.PBP`, the game uses it;
without it (or with one made for a different version) the game is in English.

You make a `LANG.PAK` from the game's own text files with `mklang.py` from the **translation kit**
(the `translation/` folder of this repository). You also need **Helltaker on PC** (free on Steam):
the kit has no game text, it reads the English text files from your own install. And **Python 3**
with **Pillow** and **numpy**, nothing else:

```
pip install pillow numpy
```

The kit contains:

```
translation/
  mklang.py         builds LANG.PAK
  htlang.py         used by mklang.py (the same text rendering as the port)
  langkit/
    slots.json      the list of texts the PSP shows (matches one version of the port)
    english/        psp.json: the PSP's own English lines (see step 2)
    fonts/          the fonts the game uses (free fonts, licences included)
  TRANSLATING.md    this guide
```

## 1. Get the text files

The text is in the PC game's own files. Open your Helltaker folder
(Steam: right click Helltaker > Manage > Browse local files):

* `local/` : the main game (`m.json` menus, `m1.json` / `m2.json` main menu and chapter titles,
  `1.json` ... `11.json` and the `h` / `_` variants: dialogue)
* `localHM/` : the Examtaker DLC (`hm_m.json`, `hm_0.json` ...)

These are **not JSON**: each line is one text, found by its line number. Keep every line where it is,
keep empty lines empty, and do not add or remove lines.

Already have a translation of the PC game? Use its `local` (and `localHM`) files as they are.

## 2. Translate

Make a folder for your language with `local/` and `localHM/` inside (or all files together in one folder)
and put the files you translated in it. Files you leave out stay English. Save as UTF-8 or UTF-16.

`m.json` line 1 (`English version of Helltaker`) is the translation credit: write your name there, for
example `Polish translation by YourName`. It is shown in the credits.

**`psp.json`** holds texts that differ on the PSP. Copy `langkit/english/psp.json` into your folder and
translate the texts, keeping `[X]`, `[L]` and `[R]` (they are drawn as PSP buttons):

* `local/m.json` 2 and 3: the HUD prompts `RESTART [R]` and `[L] LIFE ADVICE`
* `local/m1.json` 20: `Press [X] to continue.`
* `local/m.json` 35: `LANGUAGE`, the pause menu option that switches between your translation and English
* `local/m.json` 36: `PSP port by SurelyNotVain` (the port's credit)

The numbers are line numbers counted from 0 (line 1 in your editor is `"0"`).

## 3. Build LANG.PAK

From the kit folder:

```
python mklang.py MyLanguage --name Polski
```

mklang finds your Helltaker install by itself in the usual Steam folders. If it says
`English text files not found`, tell it where the game is (the folder that has `local` and `localHM`):

```
python mklang.py MyLanguage --name Polski --game "D:\Games\Steam\steamapps\common\Helltaker"
```

`--name` is your language's own name: the game shows it at start-up (ENGLISH / POLSKI) and in the
pause menu (`LANGUAGE: POLSKI`). Read what mklang prints:

* `has N lines, English has M`: a line was added or removed; fix the file and build again
* `psp.json: still English`: the lines listed above are not translated yet
* `characters are not in the game font`: the font has no such letters (see section 5)

## 4. Install and use

Copy `LANG.PAK` next to `EBOOT.PBP` (`PSP/GAME/HelltakerPSP/`). When the game starts it asks for the
language (D-pad or stick to choose, X to confirm); START in the main menu asks again, and the pause menu
gets a **LANGUAGE** option that switches between the translation and English at any time. The choice is saved.
Delete `LANG.PAK` to remove the translation.

## 5. Good to know

* The game font covers the Latin alphabets with accents (and more), so most European languages work.
  Letters it does not have show as boxes.
* Long dialogue lines are split over two lines where the English text is, otherwise the text is made a
  little smaller to fit. Check your translation on a PSP or in PPSSPP.
* Text that is part of the artwork stays English, as in the PC game (for example the logo and BAD END).
* A `LANG.PAK` belongs to the port version whose kit built it. Rebuild it with the new kit when a new
  version comes out (your text files stay the same).
