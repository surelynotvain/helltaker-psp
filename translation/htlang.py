"""Text rendering shared by the converter (pack.py, built-in English) and mklang.py (LANG.PAK translations).

The PSP port draws every text it knows at convert time as a pre-rendered picture, found at run time by
FNV-1a(font, English string).  A LANG.PAK carries pictures of the translated texts under the same keys,
so the game shows the translation wherever it would show the English text.

LANG.PAK layout (little endian, offsets from the start of the file):
  header  "HTLG" u32 version, u32 kit, u32 nent, u32 npages, u32 ent, u32 pages, char name[32]
  Ent     {u32 hash; u8 font, page; u16 u, v, w, h; s16 ox, oy; u16 adv}   sorted by (hash, font); adv px*16
  Page    {u16 w, h; u32 data, clut}   8-bit, PSP-swizzled, CLUT 256 x RGBA8888
Only Pillow and numpy are needed."""
import json, os, re, struct, zlib
import numpy as np
from PIL import Image, ImageDraw, ImageFont

FONTS = [("FONT_TEXT", "CrimsonPro-Medium", 13), ("FONT_NAME", "CrimsonPro-Medium", 14),
         ("FONT_SMALL", "CrimsonPro-Medium", 12), ("FONT_BIG", "Amiri-Regular", 34), ("FONT_TITLE", "CrimsonPro-Medium", 20),
         ("FONT_MENU", "CrimsonPro-Medium", 17), ("FONT_SEG", "Segment7Standard", 25),
         ("FONT_SCORE", "CrimsonPro-Medium", 45)]
# widest line each font may draw on the PSP; longer strings are rendered at a smaller size
FONT_MAXW = {"FONT_TEXT": 468, "FONT_NAME": 468, "FONT_SMALL": 400, "FONT_TITLE": 400, "FONT_MENU": 300}
FONT_ID = {f[0]: i for i, f in enumerate(FONTS)}
FONT_FILES = ("CrimsonPro-Medium", "Amiri-Regular", "Segment7Standard")

MAGIC = b"HTLG"
VERSION = 1
HDR = "<4sIIIIII32s"


def kit_id():
    """a LANG.PAK only fits the port versions that render text the same way"""
    return zlib.crc32(json.dumps([FONTS, sorted(FONT_MAXW.items())]).encode()) & 0xFFFFFFFF


class Fonts:
    def __init__(self, fontdir):
        self.dir = fontdir
        self.cache = {}

    def get(self, fi, size=None):
        key = (fi, size or FONTS[fi][2])
        if key not in self.cache:
            self.cache[key] = ImageFont.truetype(os.path.join(self.dir, FONTS[fi][1] + ".ttf"), key[1])
        return self.cache[key]

    def path(self, fi):
        return os.path.join(self.dir, FONTS[fi][1] + ".ttf")


# ---------------------------------------------------------------- PSP wording
PSP_BUTTONS = {"[ENTER or A]": "[X]", "[R or RB]": "[R]", "[L or LB]": "[L]"}
BUTTON_TOKEN = re.compile(r"\[(X|L|R)\]")


def psp_text(t):
    for a, b in PSP_BUTTONS.items():
        t = t.replace(a, b)
    # HUD prompts: no "•" ornaments; the L button leads ("[L] LIFE ADVICE"), the R button trails
    if t.startswith("•") and t.endswith("•") and BUTTON_TOKEN.search(t):
        t = t.strip("• ")
        if t.endswith("[L]"):
            t = "[L] " + t[:-3].rstrip()
    return t


def fnv(font, text):
    h = 2166136261 ^ font
    for b in text.encode("utf-8"):
        h = ((h ^ b) * 16777619) & 0xFFFFFFFF
    return h


def wrap_line(t, fi, maxw, fonts):
    """split at the space that best balances two lines"""
    f = fonts.get(fi)
    if f.getlength(t) <= maxw or " " not in t:
        return [t]
    return split_balanced(t, f)


def split_balanced(t, f):
    best = None
    for i, ch in enumerate(t):
        if ch == " ":
            a, b = t[:i], t[i + 1:]
            score = max(f.getlength(a), f.getlength(b))
            if best is None or score < best[0]:
                best = (score, [a, b])
    return best[1] if best else [t]


def wrap_into(t, fi, n, fonts):
    """a translated line in exactly n parts (the English layout has n lines there)"""
    if n <= 1:
        return [t]
    parts = split_balanced(t, fonts.get(fi)) if " " in t else [t]
    parts += [""] * (n - len(parts))
    return parts[:n]


# ---------------------------------------------------------------- rendering
BUTTON_RE = re.compile(r"\[(X|L|R)\]")
BTN_FILL, BTN_RIM, CROSS_BLUE = (38, 36, 46, 255), (236, 236, 236, 255), (124, 170, 232, 255)


def button_icon(kind, h, fi, fonts):
    """PSP-style button, h pixels tall (supersampled 4x)."""
    k = 4
    H = h * k
    W = H if kind == "X" else int(H * 1.7)
    im = Image.new("RGBA", (W + 2 * k, H + 2 * k), (0, 0, 0, 0))
    d = ImageDraw.Draw(im)
    rim = max(k, H // 9)
    if kind == "X":
        d.ellipse((k, k, k + W, k + H), fill=BTN_FILL, outline=BTN_RIM, width=rim)
        m = int(H * 0.3)
        lw = max(k, H // 7)
        d.line((k + m, k + m, k + W - m, k + H - m), fill=CROSS_BLUE, width=lw)
        d.line((k + W - m, k + m, k + m, k + H - m), fill=CROSS_BLUE, width=lw)
    else:
        r = H // 3
        # shoulder button: rounded on top, the outer top corner more so
        d.rounded_rectangle((k, k, k + W, k + H), radius=r, fill=BTN_FILL, outline=BTN_RIM, width=rim)
        f = ImageFont.truetype(fonts.path(fi), int(H * 0.95))
        sw = k // 2   # bold stroke so the letter survives the 4x downscale
        bb = f.getbbox(kind, stroke_width=sw)
        d.text((k + (W - (bb[2] - bb[0])) / 2 - bb[0], k + (H - (bb[3] - bb[1])) / 2 - bb[1]), kind, font=f,
               fill=BTN_RIM, stroke_width=sw, stroke_fill=BTN_RIM)
    return im.resize((max(1, im.width // k), max(1, im.height // k)), Image.LANCZOS)


def render_with_buttons(text, font, fi, size, fonts):
    """-> (RGBA image, ox, oy, advance) with the same pivot convention as plain text."""
    cap = font.getbbox("X")
    ih = max(8, round((cap[3] - cap[1]) * 1.45))
    cy = (cap[1] + cap[3]) / 2
    M = 8
    canvas = Image.new("RGBA", (int(font.getlength(text)) + 8 * ih + 2 * M, size * 3 + 2 * M), (255, 255, 255, 0))
    mask = Image.new("L", canvas.size, 0)
    md = ImageDraw.Draw(mask)
    x = 0.0
    icons = []
    for i, part in enumerate(BUTTON_RE.split(text)):
        if i % 2 == 0:
            if part:
                md.text((M + x, M), part, font=font, fill=255)
                x += font.getlength(part)
        else:
            ic = button_icon(part, ih, fi, fonts)
            icons.append((ic, M + x + 1, M + cy - ic.height / 2))
            x += ic.width + 2
    canvas.putalpha(mask)
    for ic, ix, iy in icons:
        canvas.alpha_composite(ic, (int(round(ix)), int(round(iy))))
    box = canvas.getbbox() or (0, 0, 1, 1)
    box = (max(0, box[0] - 1), max(0, box[1] - 1), min(canvas.width, box[2] + 1), min(canvas.height, box[3] + 1))
    return canvas.crop(box), box[0] - M, box[1] - M, x


def render_text(fi, text, fonts):
    """-> (RGBA image, ox, oy, advance); pivot = pen start on the line top (y = baseline - ascent)"""
    size = FONTS[fi][2]
    maxw = FONT_MAXW.get(FONTS[fi][0], 9999)
    while size > 9 and fonts.get(fi, size).getlength(text) > maxw:
        size -= 1
    font = fonts.get(fi, size)
    if BUTTON_RE.search(text):
        return render_with_buttons(text, font, fi, size, fonts)
    bbox = font.getbbox(text) if text else (0, 0, 1, 1)
    pad = 1
    w, h = bbox[2] - bbox[0] + 2 * pad, bbox[3] - bbox[1] + 2 * pad
    im = Image.new("RGBA", (max(1, w), max(1, h)), (255, 255, 255, 0))
    mask = Image.new("L", im.size, 0)
    if text:
        ImageDraw.Draw(mask).text((pad - bbox[0], pad - bbox[1]), text, font=font, fill=255)
    im.putalpha(mask)
    return im, bbox[0] - pad, bbox[1] - pad, font.getlength(text) if text else 0


# ---------------------------------------------------------------- text files
def read_lines(path):
    """A PC text file: UTF-16 (with BOM) or UTF-8, one entry per line (File.ReadAllLines)."""
    raw = open(path, "rb").read()
    s = raw.decode("utf-16") if raw[:2] in (b"\xff\xfe", b"\xfe\xff") else raw.decode("utf-8-sig")
    return s.replace("\r\n", "\n").replace("\r", "\n").split("\n")


# ---------------------------------------------------------------- LANG.PAK
def pow2(v):
    p = 8
    while p < v:
        p <<= 1
    return p


def swizzle(data, width_bytes, height):
    a = np.frombuffer(data, np.uint8).reshape(height, width_bytes)
    return a.reshape(height // 8, 8, width_bytes // 16, 16).transpose(0, 2, 1, 3).tobytes()


def _shelf_pack(items, W=512, H=512):
    """items [(key, image)] -> pages [[(key, x, y)]], tallest first, 1px gaps"""
    order = sorted(items, key=lambda it: -it[1].height)
    pages, cur, x, y, row = [], [], 0, 0, 0
    for key, im in order:
        w, h = im.width + 1, im.height + 1
        if x + w > W:
            x, y, row = 0, y + row, 0
        if y + h > H:
            pages.append(cur)
            cur, x, y, row = [], 0, 0, 0
        cur.append((key, x, y))
        x += w
        row = max(row, h)
    if cur:
        pages.append(cur)
    return pages


def build_pak(entries, name):
    """entries [(font, english_string, image, ox, oy, adv)] -> LANG.PAK bytes"""
    imgs = {i: e[2] for i, e in enumerate(entries)}
    pages = _shelf_pack(list(imgs.items()))
    where = {}
    page_blobs = []
    for pi, placed in enumerate(pages):
        used_h = pow2(max(y + imgs[k].height + 1 for k, x, y in placed))
        sheet = Image.new("RGBA", (512, used_h), (0, 0, 0, 0))
        for k, x, y in placed:
            sheet.paste(imgs[k], (x, y))
            where[k] = (pi, x, y)
        # 256-colour page: index 0 transparent, 1..192 white at 192 alpha steps (all text), 193..255 the colours
        # of the PSP button icons (a generic quantizer smears the alpha edges of thin text)
        arr = np.asarray(sheet).astype(np.int32)
        a = arr[..., 3]
        white = (arr[..., 0] >= 235) & (arr[..., 1] >= 235) & (arr[..., 2] >= 235)
        idx = np.zeros(a.shape, np.uint8)
        wa = white & (a >= 2)
        idx[wa] = 1 + np.clip((a[wa] * 191 + 127) // 255, 0, 191)
        pal = [(255, 255, 255, 0)] + [(255, 255, 255, round(i * 255 / 191)) for i in range(192)]
        other = (~white) & (a >= 2)
        if other.any():
            px = Image.fromarray(arr[other].astype(np.uint8).reshape(1, -1, 4), "RGBA")
            q = px.quantize(colors=63, method=Image.Quantize.FASTOCTREE)
            qp = q.getpalette(rawmode="RGBA") or []
            idx[other] = 193 + np.asarray(q, np.uint8).reshape(-1)
            pal += [tuple(qp[i * 4:i * 4 + 4]) for i in range(63)]
        pal += [(0, 0, 0, 0)] * (256 - len(pal))
        clut = bytearray()
        for r, g, b, al in pal[:256]:
            if al < 4:
                r = g = b = al = 0
            clut += struct.pack("<I", r | (g << 8) | (b << 16) | (al << 24))
        page_blobs.append((512, used_h, swizzle(idx.tobytes(), 512, used_h), bytes(clut)))
    rows = []
    for i, (fi, en, im, ox, oy, adv) in enumerate(entries):
        pi, x, y = where[i]
        rows.append((fnv(fi, en), fi, pi, x, y, im.width, im.height, int(round(ox)), int(round(oy)), int(round(adv * 16))))
    rows.sort(key=lambda r: (r[0], r[1]))
    out = bytearray(struct.calcsize(HDR))
    o_ent = len(out)
    for r in rows:
        out += struct.pack("<IBBHHHHhhH", *r)
    while len(out) % 64:
        out.append(0)
    o_pages = len(out)
    out += bytes(12 * len(page_blobs))
    for i, (w, h, data, clut) in enumerate(page_blobs):
        while len(out) % 64:
            out.append(0)
        d_off = len(out)
        out += data
        while len(out) % 64:
            out.append(0)
        c_off = len(out)
        out += clut
        struct.pack_into("<HHII", out, o_pages + 12 * i, w, h, d_off, c_off)
    struct.pack_into(HDR, out, 0, MAGIC, VERSION, kit_id(), len(rows), len(page_blobs), o_ent, o_pages,
                     name.encode("utf-8")[:31])
    return bytes(out)
