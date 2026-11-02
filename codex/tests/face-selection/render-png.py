#!/usr/bin/env python3
"""Convert native LVGL framebuffers to PNG and create a preview contact sheet."""
from pathlib import Path
import sys

from PIL import Image, ImageDraw

folder = Path(sys.argv[1])
tiles = []
for ppm in sorted(folder.glob("*.ppm")):
    image = Image.open(ppm)
    image.save(ppm.with_suffix(".png"))
    image.resize((720, 720), Image.Resampling.NEAREST).save(ppm.with_name(ppm.stem + "-3x.png"))
    tile = Image.new("RGB", (260, 274), (28, 28, 28))
    tile.paste(image, (10, 26))
    ImageDraw.Draw(tile).text((10, 7), ppm.stem, fill="white")
    tiles.append(tile)
sheet = Image.new("RGB", (780, 274 * ((len(tiles) + 2) // 3)), (28, 28, 28))
for index, tile in enumerate(tiles):
    sheet.paste(tile, ((index % 3) * 260, (index // 3) * 274))
sheet.save(folder / "contact-sheet.png")
