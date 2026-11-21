#!/usr/bin/env python3
"""Regenerate the native package tile in the same restrained workbench language.

The silhouette is a dimensional application motif, not a flight photo, safety
claim or borrowed logo. Pillow is a build-only dependency; it is not packaged.
"""
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont

root = Path(__file__).resolve().parents[1]
font = root / "assets/SpaceGrotesk.ttf"
size = 512
background = (242, 244, 243)
ink = (39, 52, 59)
muted = (77, 93, 103)
line = (187, 202, 209)
blue = (35, 91, 147)

im = Image.new("RGB", (size, size), background)
d = ImageDraw.Draw(im)
# The one application motif: the actual rocket's silhouette on a measured axis.
d.rectangle((20, 20, 492, 492), outline=line, width=2)
d.rectangle((20, 20, 28, 492), fill=blue)
d.rectangle((44, 44, 218, 468), fill=(231, 237, 239))
d.line((131, 77, 131, 425), fill=line, width=2)
d.polygon([(119, 171), (131, 76), (143, 171)], fill=ink)
d.rectangle((119, 171, 143, 393), fill=ink)
d.line((119, 171, 143, 171), fill=blue, width=3)
d.polygon([(119, 320), (87, 408), (131, 390)], fill=muted)
d.polygon([(143, 320), (175, 408), (131, 390)], fill=muted)
d.line((66, 76, 66, 410), fill=line, width=2)
d.line((57, 76, 75, 76), fill=blue, width=3)
d.line((57, 410, 75, 410), fill=blue, width=3)
d.line((80, 416, 182, 416), fill=line, width=2)
d.ellipse((126, 268, 136, 278), fill=blue)
d.line((89, 273, 122, 273), fill=blue, width=3)

headline = ImageFont.truetype(str(font), 51)
section = ImageFont.truetype(str(font), 26)
caption = ImageFont.truetype(str(font), 22)
d.text((244, 161), "EuloLab", font=headline, fill=ink)
d.rectangle((246, 241, 462, 244), fill=blue)
d.text((244, 263), "ROCKET STUDIO", font=section, fill=ink)
d.text((244, 311), "DESIGN / ANALYSIS", font=caption, fill=muted)
d.text((244, 392), "EDUCATIONAL MODEL", font=caption, fill=muted)
out = root / "assets/icon0.png"
im.save(out, optimize=True)
print(out, out.stat().st_size, "bytes")
