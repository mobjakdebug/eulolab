#!/usr/bin/env python3
"""Produce the package icon with Pillow (build-time only)."""
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont

root = Path(__file__).resolve().parents[1]
size = 512
im = Image.new("RGB", (size, size), (10, 20, 29))
d = ImageDraw.Draw(im)
for i in range(0, size, 24):
    d.line([(i, 0), (i, size)], fill=(22, 40, 48), width=1)
d.rounded_rectangle((37, 37, 475, 475), radius=60, outline=(70, 110, 118), width=3)
d.ellipse((90, 84, 422, 416), outline=(65, 130, 132), width=3)
d.ellipse((120, 114, 392, 386), outline=(37, 83, 91), width=3)
font = ImageFont.truetype(str(root / "assets/SpaceGrotesk.ttf"), 171)
d.text((115, 118), "E", font=font, fill=(255, 175, 104))
small = ImageFont.truetype(str(root / "assets/SpaceGrotesk.ttf"), 40)
d.text((111, 342), "EULO / LAB", font=small, fill=(224, 235, 229))
out = root / "assets/icon0.png"
im.save(out, optimize=True)
print(out, out.stat().st_size, "bytes")
