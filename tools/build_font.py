#!/usr/bin/env python3
"""Generate the font atlas used by *both* native PS4 and host renderers.

Source font: Space Grotesk, SIL OFL 1.1 (assets/OFL.txt). The output is a
simple, endian-independent array of one-byte advances and alpha cells.
Pillow is a build-time tool only; neither platform depends on it at runtime.
"""
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parents[1]
FONT = ROOT / "assets/SpaceGrotesk.ttf"
OUT = ROOT / "assets/font-atlas.bin"
# Compact workbench labels and section titles at 1280x720.
STYLES = [(17, 27, 31), (27, 43, 49)]  # point size, cell width, cell height


def main() -> None:
    data = bytearray(b"EULOFNT1")
    for size, width, height in STYLES:
        data += bytes((width, height))
        face = ImageFont.truetype(str(FONT), size)
        ascent, _ = face.getmetrics()
        for code in range(32, 127):
            ch = chr(code)
            canvas = Image.new("L", (width, height), 0)
            ImageDraw.Draw(canvas).text((2, min(ascent + 2, height - 5)), ch, font=face, fill=255, anchor="ls")
            advance = max(4, min(width, round(face.getlength(ch)) + 1))
            data.append(advance)
            data.extend(canvas.tobytes())
    OUT.write_bytes(data)
    print(f"Generated {OUT.relative_to(ROOT)} ({len(data)} bytes) from {FONT.name}")


if __name__ == "__main__":
    main()
