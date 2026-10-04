"""Render exported Anju archives at matching cameras for seated fit review."""

from pathlib import Path
import argparse
import functools
import io

from PIL import Image, ImageDraw, ImageFont

import render_anju_preview as preview

BACKGROUND = (22, 29, 42)
FONT = "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf"


def sheet(native, archives, labels, title, destination):
    image = Image.new("RGB", (1440, 1310), BACKGROUND)
    draw = ImageDraw.Draw(image)
    heading = ImageFont.truetype(FONT, 25)
    text = ImageFont.truetype(FONT, 19)
    draw.text((20, 15), title, font=heading, fill="#eef2fa")
    for column, (archive, label) in enumerate(zip(archives, labels)):
        model = preview.Anju(native, False, archive)
        parts = [
            p for p in model.pose(model.animation, 0) if "Umbrella" not in p["path"]
        ]
        for row, yaw in enumerate((5, 100)):
            panel = preview.render(
                parts, (720, 580), yaw=yaw, scale=0.11, center_y=2350
            )
            image.paste(panel, (column * 720, 80 + row * 600))
            draw.text(
                (column * 720 + 20, 55 + row * 600),
                label + " | " + ("Front" if row == 0 else "Side"),
                font=text,
                fill="#e6c479",
            )
    draw.text(
        (20, 1268),
        "Actual archive readback | Native crying frame 0 | Umbrella hidden for inspection | Offline lighting",
        font=text,
        fill="#aebcd0",
    )
    payload = io.BytesIO()
    image.save(payload, format="PNG")
    destination.write_bytes(payload.getvalue())
    Image.open(destination).verify()
    print(destination, flush=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("native", type=Path)
    parser.add_argument("r8_auburn", type=Path)
    parser.add_argument("r9_auburn", type=Path)
    parser.add_argument("r9_goth", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    preview.read_texture = functools.lru_cache(maxsize=32)(preview.read_texture)
    sheet(
        args.native,
        [args.r8_auburn, args.r9_auburn],
        ["R8 recovered", "R9 rebuilt"],
        "ANJU | Seated skirt and forearm comparison",
        args.output / "Anju_R8_R9_Seated_Comparison.png",
    )
    sheet(
        args.native,
        [args.r9_auburn, args.r9_goth],
        ["Auburn", "Goth"],
        "ANJU R9 | Fitted seated skirt and connected forearm",
        args.output / "Anju_R9_Seated_Palettes.png",
    )


if __name__ == "__main__":
    main()
