"""Builds the five Sheikah Slate kaleido icons from the BotW model render (Skijer's NEI).

The slate cell is never drawn from the bare slate texture in practice: obtaining any rune grants
the slate, so ExtInv_GetItemIcon always answers Slate_RuneIcon() — the four rune COMPOSITES. They
must therefore be built on the same art as the standalone icon, or the cell shows a different
object from the one the get-item textbox showed.

Base art: the render's alpha is 1-bit apart from the store watermark, which sits at alpha 1 over
the whole canvas — hence ALPHA_FLOOR, without which the watermark survives the downscale as a
blue haze.

Usage:  python generate_slate_icons.py [path/to/BotW_Sheikah_Slate_Model.png]
"""

import os
import sys

from PIL import Image

DEFAULT_SRC = "BotW_Sheikah_Slate_Model.png"

OUTPUT_DIR = os.path.dirname(os.path.abspath(__file__))

ICON = 32
ALPHA_FLOOR = 8

# The composite reserves the bottom-right corner for the rune badge, so its slate is fitted into a
# smaller box, so the badge overlaps its corner instead of hiding it. Both stay CENTRED in the
# cell: the badge is drawn over the slate, it does not push it aside.
COMPOSITE_BOX = (24, 28)
BADGE = 15

# Sensor keeps the Desire Sensor's own icon as its badge: the rune IS that item, rehoused.
RUNES = {
    "Bomb": "gItemIconSlateRuneBombTex",
    "Stasis": "gItemIconSlateRuneStasisTex",
    "Cryonis": "gItemIconSlateRuneCryonisTex",
    "MasterCycle": "gItemIconSlateRuneMasterCycleTex",
    "Sensor": "gItemIconDesireSensorTex",
}


def load_trimmed(path):
    """Opaque pixels only, cropped to their bounding box."""
    img = Image.open(path).convert("RGBA")
    alpha = img.getchannel("A").point(lambda v: v if v >= ALPHA_FLOOR else 0)
    img.putalpha(alpha)
    return img.crop(img.getbbox())


def fit(img, box):
    """Scale to fit `box` keeping the aspect ratio."""
    scale = min(box[0] / img.width, box[1] / img.height)
    return img.resize((max(1, round(img.width * scale)), max(1, round(img.height * scale))), Image.LANCZOS)


def centre(sprite):
    canvas = Image.new("RGBA", (ICON, ICON), (0, 0, 0, 0))
    canvas.alpha_composite(sprite, ((ICON - sprite.width) // 2, (ICON - sprite.height) // 2))
    return canvas


def main():
    src = sys.argv[1] if len(sys.argv) > 1 else DEFAULT_SRC
    if not os.path.isfile(src):
        sys.exit(f"slate render not found: {src}")

    slate = load_trimmed(src)

    standalone = fit(slate, (ICON, ICON))
    centre(standalone).save(os.path.join(OUTPUT_DIR, "gItemIconSheikahSlateTex.rgba32.png"))
    print(f"wrote gItemIconSheikahSlateTex.rgba32.png  ({standalone.width}x{standalone.height} slate)")

    small = fit(slate, COMPOSITE_BOX)
    for rune, badgeName in RUNES.items():
        badge = fit(load_trimmed(os.path.join(OUTPUT_DIR, f"{badgeName}.rgba32.png")), (BADGE, BADGE))
        composite = centre(small)
        composite.alpha_composite(badge, (ICON - badge.width, ICON - badge.height))
        out = f"gItemIconSheikahSlate{rune}Tex.rgba32.png"
        composite.save(os.path.join(OUTPUT_DIR, out))
        print(f"wrote {out}  <- slate + {badgeName}")


if __name__ == "__main__":
    main()
