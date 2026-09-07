"""Builds the four Rod of Seasons wheel icons from the staff's own coin textures (Skijer's NEI).

The model (magic_staff_3d) carries four removable coins — Clover, Sun, Buttons, Hex — one per
season, each with a 32x32 texture. Those ARE the season emblems, so the wheel shows the very coin
the rod is wearing. The only edit is cutting away the brown backing square, which exists to sit
against the staff's wood and reads as a stray box in a menu.

Usage:  python generate_season_icons.py [path/to/magic_staff_3d/low_poly/textures]
"""

import os
import sys
from collections import deque

from PIL import Image

DEFAULT_SRC = os.path.join("magic_staff_3d", "low_poly", "textures")

# Coin -> season. Oracle of Seasons' own reading: clover spring, red sun summer, gold autumn,
# blue winter. Keep in sync with sSeasonColor in mods/extended_inventory.c.
COINS = {
    "Spring": "clover_32.png",
    "Summer": "sun_32.png",
    "Autumn": "buttons_32.png",
    "Winter": "hex_32.png",
}


def cut_backing(img):
    """Clears the backing square: every pixel matching the corner colour that the border reaches.

    Flood-filled rather than colour-matched, so the same brown used inside the coin face survives.
    """
    img = img.convert("RGBA")
    w, h = img.size
    px = img.load()
    backing = px[0, 0][:3]

    seen = [[False] * h for _ in range(w)]
    queue = deque()
    for x in range(w):
        for y in (0, h - 1):
            queue.append((x, y))
    for y in range(h):
        for x in (0, w - 1):
            queue.append((x, y))

    while queue:
        x, y = queue.popleft()
        if not (0 <= x < w and 0 <= y < h) or seen[x][y]:
            continue
        seen[x][y] = True
        if px[x, y][:3] != backing:
            continue
        px[x, y] = (0, 0, 0, 0)
        queue.extend(((x + 1, y), (x - 1, y), (x, y + 1), (x, y - 1)))

    return img


def blank_coin(img):
    """The 'off' coin: a real coin with its emblem wiped and the whole thing dimmed.

    Built from a season coin rather than drawn fresh, so it keeps the exact silhouette and rim the
    other four have and reads as the same object, switched off. The emblem is whatever is neither
    the cream face nor the gold rim — flooding it with the face colour blanks the coin.
    """
    img = img.copy()
    px = img.load()
    w, h = img.size

    kept = [px[x, y][:3] for x in range(w) for y in range(h) if px[x, y][3] > 0]
    face = max(set(kept), key=kept.count)
    rim = max(set(c for c in kept if c != face), key=kept.count)

    for x in range(w):
        for y in range(h):
            r, g, b, a = px[x, y]
            if a == 0:
                continue
            if (r, g, b) not in (face, rim):
                r, g, b = face
            px[x, y] = (r // 5, g // 5, b // 5 + 6, a)
    return img


def main():
    src = sys.argv[1] if len(sys.argv) > 1 else DEFAULT_SRC
    if not os.path.isdir(src):
        sys.exit(f"coin textures not found: {src}")

    for season, coin in COINS.items():
        icon = cut_backing(Image.open(os.path.join(src, coin)))
        out = f"gItemIconSeason{season}Tex.rgba32.png"
        icon.save(out)
        print(f"wrote {out}  <- {coin}")

    off = blank_coin(cut_backing(Image.open(os.path.join(src, COINS["Spring"]))))
    off.save("gItemIconSeasonOffTex.rgba32.png")
    print("wrote gItemIconSeasonOffTex.rgba32.png  <- blanked + dimmed coin")


if __name__ == "__main__":
    main()
