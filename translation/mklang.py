#!/usr/bin/env python3
"""Build a LANG.PAK (a translation) for the Helltaker PSP port.

    python mklang.py MYLANG_FOLDER [-o LANG.PAK] [--name Polski]

MYLANG_FOLDER holds the game's text files, translated: the same files as Helltaker/local and
Helltaker/localHM on PC (1.json, m.json, m1.json, ... hm_m.json; one entry per line, UTF-16 or UTF-8),
either in local/ and localHM/ subfolders or all together. Optionally psp.json: the PSP wording of
lines that name PC keys, and the lines the port adds (see langkit/english/psp.json).
Anything missing is taken from the English files: langkit/english/local and localHM when the kit has
them, otherwise the ones in your own Helltaker install (found automatically, or --game FOLDER).

Copy the resulting LANG.PAK next to EBOOT.PBP. In game: pause menu > LANGUAGE switches between the
translation and English. Needs Python 3 with Pillow and numpy (pip install pillow numpy)."""
import argparse, json, os, sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import htlang


def find(folder, rel):
    """local/1.json -> folder/local/1.json, or folder/1.json for translations shipped flat"""
    for p in (os.path.join(folder, rel), os.path.join(folder, os.path.basename(rel))):
        if os.path.exists(p):
            return p
    return None


# where the PC game usually is: its folder holds local/ and localHM/ (the English text files)
GAME_DIRS = [os.environ.get("HELLTAKER_DIR"),
             r"C:\Program Files (x86)\Steam\steamapps\common\Helltaker",
             r"C:\Program Files\Steam\steamapps\common\Helltaker",
             os.path.expanduser("~/.local/share/Steam/steamapps/common/Helltaker"),
             os.path.expanduser("~/.steam/steam/steamapps/common/Helltaker"),
             os.path.expanduser("~/.var/app/com.valvesoftware.Steam/.local/share/Steam/steamapps/common/Helltaker"),
             os.path.expanduser("~/Library/Application Support/Steam/steamapps/common/Helltaker")]


def english_folder(kit, game):
    """the folder with the English local/ and localHM/: the kit's own copy, --game, or a Steam install"""
    for d in [os.path.join(kit, "english"), game] + GAME_DIRS:
        if d and os.path.exists(os.path.join(d, "local", "m.json")) and os.path.isdir(os.path.join(d, "localHM")):
            return d
    sys.exit("English text files not found. Point mklang at your Helltaker install (the folder with local/ and\n"
             "localHM/ in it; Steam: right click Helltaker > Manage > Browse local files):\n"
             "    python mklang.py MyLanguage --name MyLanguage --game \"PATH/TO/Helltaker\"\n"
             "or copy those two folders into langkit/english/.")


def main():
    ap = argparse.ArgumentParser(description="Build LANG.PAK for the Helltaker PSP port")
    ap.add_argument("folder", help="folder with the translated text files")
    ap.add_argument("-o", "--out", default="LANG.PAK")
    ap.add_argument("--name", default=None, help="language name shown at start-up and in the pause menu (default: folder name)")
    ap.add_argument("--kit", default=os.path.join(HERE, "langkit"), help="langkit folder (default: next to this script)")
    ap.add_argument("--game", default=None, help="your Helltaker install, the folder with local/ and localHM/ (found automatically on Steam)")
    a = ap.parse_args()

    kit = json.load(open(os.path.join(a.kit, "slots.json"), encoding="utf-8"))
    if kit["kit"] != htlang.kit_id():
        sys.exit("langkit and mklang.py are from different versions")
    psp_en = json.load(open(os.path.join(a.kit, "english", "psp.json"), encoding="utf-8"))
    english_dir = english_folder(a.kit, a.game)
    p = os.path.join(a.folder, "psp.json")
    psp_mine = json.load(open(p, encoding="utf-8")) if os.path.exists(p) else {}
    fonts = htlang.Fonts(os.path.join(a.kit, "fonts"))
    name = a.name or os.path.basename(os.path.normpath(a.folder))

    files, problems, found = {}, [], 0
    def lines_of(rel):
        nonlocal found
        if rel not in files:
            en = htlang.read_lines(os.path.join(english_dir, rel))
            p = find(a.folder, rel)
            if p:
                found += 1
                tr = htlang.read_lines(p)
                while len(tr) > len(en) and tr[-1] == "":
                    tr.pop()
                if len(tr) < len(en) and not any(en[len(tr):]):
                    tr += [""] * (len(en) - len(tr))
                if len(tr) != len(en):
                    problems.append("%s has %d lines, English has %d: lines are matched by number" % (rel, len(tr), len(en)))
                    tr = (tr + en[len(tr):])[:len(en)]
            else:
                tr = en
            files[rel] = (en, tr)
        return files[rel]

    def translated(rel, line):
        """the translated line as the PSP shows it (psp.json first)"""
        mine = psp_mine.get(rel, {}).get(str(line))
        if mine is not None:
            return mine
        en_psp = psp_en.get(rel, {}).get(str(line))
        en, tr = lines_of(rel)
        if en_psp is not None:
            # a PC-key line or a line the port adds: without the translation's psp.json it stays English
            if find(a.folder, rel) or line >= len(en):
                untranslated.add("%s line %d" % (rel, line))
            return en_psp
        return tr[line] if 0 <= line < len(tr) else ""

    untranslated = set()
    entries, done, used_chars = [], set(), set()
    for fi, en_str, rel, line, part, parts in kit["slots"]:
        if (fi, en_str) in done:
            continue
        done.add((fi, en_str))
        if rel == "lang_row":
            t = "%s: %s" % (translated("local/m.json", 35), name.upper())
        elif rel == "lang_name":
            t = name.upper()
        else:
            if line < 0:
                continue
            t = htlang.psp_text(translated(rel, line))
        t = htlang.wrap_into(t, fi, parts, fonts)[part] if parts > 1 else t
        if t == en_str:
            continue                     # same as English: the built-in picture is used
        used_chars |= set(t)
        im, ox, oy, adv = htlang.render_text(fi, t, fonts)
        entries.append((fi, en_str, im, ox, oy, adv))
    if not found:
        sys.exit("no translated text files found in " + a.folder)

    data = htlang.build_pak(entries, name)
    open(a.out, "wb").write(data)
    print("wrote %s (%s, %d texts, %d KB)" % (a.out, name, len(entries), len(data) // 1024))
    if untranslated:
        problems.append("psp.json: still English: " + ", ".join(sorted(untranslated)) +
                        " (copy langkit/english/psp.json to your folder and translate it; [X] [L] [R] are PSP buttons)")
    missing = sorted(c for c in used_chars if not has_glyph(fonts, c))
    if missing:
        problems.append("%d characters are not in the game font and show as boxes: %s" % (len(missing), "".join(missing)[:200]))
    for p in problems:
        print("WARNING:", p)


def has_glyph(fonts, ch):
    if ch in " \t":
        return True
    f = fonts.get(0, 32)
    nd = f.getmask("\U0010FFFD")
    m = f.getmask(ch)
    return m.getbbox() is not None and (m.size, bytes(m)) != (nd.size, bytes(nd))


if __name__ == "__main__":
    main()
