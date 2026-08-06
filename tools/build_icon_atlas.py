#!/usr/bin/env python3
"""Builds the skill-icon atlas from game-icons.net source SVGs.

    python3 tools/build_icon_atlas.py [--fetch]

Reads assets/icons/skills.manifest, rasterises each icon to a white
silhouette on transparency, and packs them into assets/icons/skills.png.
The runtime tints that silhouette per skill, so one greyscale atlas
covers every school colour instead of thirty coloured images.

The generated atlas IS committed, so building the game needs no Python,
no network and no SVG rasteriser - same deal as the bundled font. This
script only has to run when the icon list changes.

--fetch downloads any SVG missing from assets/icons/svg/. Without it the
script works entirely offline from the committed sources.
"""

import argparse
import os
import re
import struct
import subprocess
import sys
import zlib

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
MANIFEST = os.path.join(ROOT, "assets", "icons", "skills.manifest")
SVG_DIR = os.path.join(ROOT, "assets", "icons", "svg")
ATLAS_PNG = os.path.join(ROOT, "assets", "icons", "skills.png")
ATLAS_META = os.path.join(ROOT, "assets", "icons", "skills.atlas")
SKILL_H = os.path.join(ROOT, "src", "skill.h")
CREDITS = os.path.join(ROOT, "assets", "CREDITS.md")

SOURCE_URL = "https://raw.githubusercontent.com/game-icons/icons/master/{}.svg"

CELL = 128   # pixels per icon cell; 128 stays sharp on a 4K skill bar
COLS = 8     # atlas columns - 30 icons lands in 4 rows of 8

# The 512x512 background square every game-icons SVG opens with. It has
# to go, or every icon rasterises to a solid block.
BG_PATH = re.compile(r'<path[^>]*\sd="M0 0h512v512H0z"[^>]*/>')


def read_manifest():
    entries = []
    with open(MANIFEST, encoding="utf-8") as f:
        for line in f:
            line = line.strip()
            if not line or line.startswith("#"):
                continue
            skill, icon = line.split()
            entries.append((skill, icon))
    return entries


def read_skill_ids():
    """The SkillId enum, in declaration order, straight from the header."""
    src = open(SKILL_H, encoding="utf-8").read()
    body = re.search(r"typedef enum \{(.*?)\} SkillId;", src, re.S)
    if not body:
        sys.exit("could not find the SkillId enum in src/skill.h")
    ids = []
    for raw in body.group(1).split(","):
        raw = re.sub(r"//.*", "", raw).strip()
        name = raw.split("=")[0].strip()
        if name.startswith("SK_") and name != "SK_COUNT":
            ids.append(name)
    return ids


def check_in_sync(entries):
    """The atlas is indexed BY SkillId, so a mismatch would silently put
    the wrong picture on every skill after the first divergence."""
    want = read_skill_ids()
    got = [skill for skill, _ in entries]
    if got == want:
        return
    print("manifest and SkillId enum disagree:", file=sys.stderr)
    for i in range(max(len(want), len(got))):
        w = want[i] if i < len(want) else "-"
        g = got[i] if i < len(got) else "-"
        if w != g:
            print("  slot %2d: skill.h has %-22s manifest has %s" % (i, w, g),
                  file=sys.stderr)
    sys.exit(1)


def fetch(icon):
    dest = os.path.join(SVG_DIR, icon + ".svg")
    if os.path.exists(dest):
        return dest
    os.makedirs(os.path.dirname(dest), exist_ok=True)
    url = SOURCE_URL.format(icon)
    print("  fetching", icon)
    subprocess.run(["curl", "-sL", "--max-time", "40", "--fail", url, "-o", dest],
                   check=True)
    return dest


def rasterise(icon):
    """One icon -> (CELL x CELL) RGBA bytes, white shape on transparency."""
    path = os.path.join(SVG_DIR, icon + ".svg")
    if not os.path.exists(path):
        sys.exit("missing %s - re-run with --fetch" % path)
    svg = open(path, encoding="utf-8").read()

    stripped, n = BG_PATH.subn("", svg)
    if n == 0:
        print("  note: %s had no background square to strip" % icon)
    # Force white on whatever is left: most icons declare fill="#fff"
    # themselves, but the ones that inherit would come out black.
    stripped = stripped.replace("<svg ", '<svg fill="#ffffff" ', 1)

    out = subprocess.run(
        ["rsvg-convert", "-w", str(CELL), "-h", str(CELL), "-f", "png"],
        input=stripped.encode("utf-8"), stdout=subprocess.PIPE, check=True).stdout
    return decode_png_rgba(out)


# --- Minimal PNG I/O. Pillow isn't installed and this needs so little of
# the format that a dependency would cost more than it saves. ----------

def decode_png_rgba(blob):
    pos, w, h, depth, ctype, idat = 8, 0, 0, 0, 0, b""
    while pos < len(blob):
        ln = struct.unpack(">I", blob[pos:pos + 4])[0]
        tag = blob[pos + 4:pos + 8]
        data = blob[pos + 8:pos + 8 + ln]
        if tag == b"IHDR":
            w, h, depth, ctype = struct.unpack(">IIBB", data[:10])
        elif tag == b"IDAT":
            idat += data
        pos += 12 + ln
    if depth != 8 or ctype != 6:
        sys.exit("expected 8-bit RGBA from rsvg-convert, got depth %d type %d"
                 % (depth, ctype))

    raw = zlib.decompress(idat)
    stride = w * 4
    out = bytearray(h * stride)
    prev = bytearray(stride)
    p = 0
    for y in range(h):
        ft = raw[p]; p += 1
        line = bytearray(raw[p:p + stride]); p += stride
        if ft == 1:
            for i in range(4, stride):
                line[i] = (line[i] + line[i - 4]) & 0xFF
        elif ft == 2:
            for i in range(stride):
                line[i] = (line[i] + prev[i]) & 0xFF
        elif ft == 3:
            for i in range(stride):
                left = line[i - 4] if i >= 4 else 0
                line[i] = (line[i] + ((left + prev[i]) >> 1)) & 0xFF
        elif ft == 4:
            for i in range(stride):
                a = line[i - 4] if i >= 4 else 0
                b = prev[i]
                c = prev[i - 4] if i >= 4 else 0
                pp = a + b - c
                pa, pb, pc = abs(pp - a), abs(pp - b), abs(pp - c)
                pr = a if (pa <= pb and pa <= pc) else (b if pb <= pc else c)
                line[i] = (line[i] + pr) & 0xFF
        elif ft != 0:
            sys.exit("unsupported PNG filter %d" % ft)
        out[y * stride:(y + 1) * stride] = line
        prev = line
    return w, h, out


def encode_png_rgba(w, h, pixels):
    raw = bytearray()
    stride = w * 4
    for y in range(h):
        raw.append(0)  # filter: none - the atlas is mostly transparent
        raw += pixels[y * stride:(y + 1) * stride]

    def chunk(tag, data):
        return (struct.pack(">I", len(data)) + tag + data +
                struct.pack(">I", zlib.crc32(tag + data) & 0xFFFFFFFF))

    return (b"\x89PNG\r\n\x1a\n"
            + chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 6, 0, 0, 0))
            + chunk(b"IDAT", zlib.compress(bytes(raw), 9))
            + chunk(b"IEND", b""))


def write_credits(entries):
    """CC BY 3.0 credits the ARTIST, not the site, so the author list is
    derived from what's actually used rather than maintained by hand."""
    authors = sorted({icon.split("/")[0] for _, icon in entries})
    lines = [
        "# Asset credits",
        "",
        "Everything bundled in `assets/` and who made it. Generated in part",
        "by `tools/build_icon_atlas.py` - edit that, or the manifest, rather",
        "than the icon section below.",
        "",
        "## Skill icons - `assets/icons/`",
        "",
        "Icons from [game-icons.net](https://game-icons.net), used under the",
        "[Creative Commons Attribution 3.0](https://creativecommons.org/licenses/by/3.0/)",
        "licence. CC BY credits the individual artist, so:",
        "",
    ]
    for a in authors:
        used = [i for _, i in entries if i.startswith(a + "/")]
        lines.append("- Icons made by **%s**, available on https://game-icons.net (%d used)"
                     % (a, len(used)))
    lines += [
        "",
        "The same credit is shown in-game on the title screen, which is how",
        "CC BY asks a video game to carry it. The exact icon used for each",
        "skill is listed in",
        "`skills.manifest`; the unmodified source SVGs are kept in",
        "`icons/svg/` so the atlas can be rebuilt from scratch.",
        "Modification: each icon's background square was removed and the",
        "silhouette rasterised to a shared atlas, which the game tints.",
        "",
        "## Fonts - `assets/fonts/`",
        "",
        "- **Cinzel** by Natanael Gama / The Cinzel Project Authors -",
        "  [SIL Open Font License 1.1](fonts/OFL-Cinzel.txt). Display face:",
        "  titles, panel headers, zone names.",
        "- **Alegreya Sans** by Juan Pablo del Peral / Huerta Tipografica -",
        "  [SIL Open Font License 1.1](fonts/OFL-AlegreyaSans.txt). Body face:",
        "  everything dense and small.",
        "- **DejaVu Sans** - [DejaVu Fonts License](fonts/LICENSE-DejaVu.txt).",
        "  Kept as the fallback when a bundled face is missing.",
        "",
        "## Ground textures - `assets/ground/`",
        "",
        "- Patterns from **Kenney** ([kenney.nl](https://kenney.nl)), released under",
        "  [CC0 1.0](ground/LICENSE-Kenney.txt) - public domain, no attribution",
        "  required. Credited anyway, because it's free to do and he earned it.",
        "",
    ]
    open(CREDITS, "w", encoding="utf-8").write("\n".join(lines))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--fetch", action="store_true",
                    help="download SVGs missing from assets/icons/svg/")
    args = ap.parse_args()

    entries = read_manifest()
    check_in_sync(entries)
    print("%d icons, in sync with the SkillId enum" % len(entries))

    if args.fetch:
        for _, icon in entries:
            fetch(icon)

    rows = (len(entries) + COLS - 1) // COLS
    aw, ah = COLS * CELL, rows * CELL
    atlas = bytearray(aw * ah * 4)

    for idx, (skill, icon) in enumerate(entries):
        w, h, px = rasterise(icon)
        if (w, h) != (CELL, CELL):
            sys.exit("%s rasterised to %dx%d, expected %d" % (icon, w, h, CELL))
        cx, cy = (idx % COLS) * CELL, (idx // COLS) * CELL
        for y in range(CELL):
            dst = ((cy + y) * aw + cx) * 4
            atlas[dst:dst + CELL * 4] = px[y * CELL * 4:(y + 1) * CELL * 4]
        print("  [%2d] %-22s %s" % (idx, skill, icon))

    open(ATLAS_PNG, "wb").write(encode_png_rgba(aw, ah, atlas))
    open(ATLAS_META, "w", encoding="utf-8").write(
        "# Generated by tools/build_icon_atlas.py - do not edit.\n"
        "cell=%d\ncols=%d\ncount=%d\n" % (CELL, COLS, len(entries)))
    write_credits(entries)

    print("wrote %s (%dx%d, %.0f KB)" % (ATLAS_PNG, aw, ah,
                                         os.path.getsize(ATLAS_PNG) / 1024.0))


if __name__ == "__main__":
    main()
