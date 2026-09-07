"""
Item Name Texture Generator — IA4-exact (Skijer's NEI).

Produces the 128x16 name-box textures the kaleido draws through
`Gfx_DrawTexQuad4b(..., G_IM_FMT_IA, 128, 16, 0)`.

WHY THE OLD VERSION LOOKED BROKEN
---------------------------------
The target format is **IA4: 4 bits per texel = 3-bit intensity (8 levels) + 1-bit
alpha (2 levels)**. The old script rendered a full RGBA image with antialiasing and
a coloured "GIMP long shadow", which the .ia4.png conversion then had to destroy:

  * measured on the shipped PNGs: 48-71 distinct alpha values and 100-149 distinct
    RGB values per texture. IA4 can store 2 and 8. Everything else was thrown away
    at conversion time by a hard threshold -> ragged, chewed-up glyph edges.
  * the shadow colour (#16202b) cannot exist in IA4 at all: there is no chroma.
    That navy already comes from the COMBINER at draw time -- the kaleido sets
    `gDPSetEnvColor(20, 30, 40, 0)` and lerps ENV->PRIM by TEXEL0 intensity. So
    intensity 0 IS the navy shadow and intensity 255 IS the white text, for free.
  * the shadow was drawn opaque (alpha 255) in FOUR diagonal directions at length
    3.0, 8 copies each = 32 stacked full-text passes. In 1-bit alpha that is not a
    shadow, it is a fat opaque blob swallowing 12px letters in a 16px box.
  * vertical placement used the PER-STRING ink bbox
    (`y = (H - (bbox[3]-bbox[1])) / 2 - bbox[1]`), so a name with a descender was
    positioned differently from one without. Measured on the shipped set: baselines
    scattered over y=0..2 and 22 of 62 textures CLIPPED against the canvas edge
    (Command Melody and Fugue of Home among them).
  * no width guard: "Demise's Destruct. MP12" reached x=3..124 of 128.

WHAT THIS VERSION DOES
----------------------
Renders something that is *exactly* representable in IA4, so the PNG you look at
is the texture that ships:

  * alpha is 1-bit by construction: the silhouette (glyph dilated by OUTLINE_PX,
    plus a DROP offset copy) is fully opaque, everything else fully transparent.
  * intensity carries the antialiasing: 0 in the outline ring (-> ENV navy in
    game), 255 in the letter core (-> PRIM white), smooth in between, quantised to
    the 8 levels IA4 actually has.
  * ONE fixed baseline for every texture, derived from the font metrics (cap
    height + descender), never from the per-string bbox. Names line up.
  * per-string auto-shrink when a name is too wide, keeping the same baseline.
  * `--verify` re-reads every PNG in the folder and reports anything that is not
    IA4-clean or that touches the canvas edge.

Font: Century Gothic Bold.
"""

import math
import os
import re
import sys

from PIL import Image, ImageDraw, ImageFilter, ImageFont

# ---------------------------------------------------------------------------
# Canvas / format
# ---------------------------------------------------------------------------
WIDTH = 128
HEIGHT = 16

# IA4 = 3-bit intensity + 1-bit alpha.
IA4_INTENSITY_LEVELS = 8

# Silhouette build-up. OUTLINE_PX is the ring around every glyph; DROP adds one
# extra offset copy so the bottom-right reads as a drop shadow like vanilla.
OUTLINE_PX = 1
DROP = (1, 1)

# Fully transparent rows/columns kept clear at the canvas edge, so the outline
# never bleeds into the quad border.
BORDER = 1

# Any glyph coverage at or above this gets an outline ring (keeps thin AA tips
# from losing their shadow).
AA_THRESHOLD = 24

# Horizontal breathing room inside the outline.
H_MARGIN = 1

# Font size search. The largest size whose (cap height + descender) fits the
# vertical budget wins, and that size is shared by every texture.
#
# LETTER_SPACING stays at 0. The old -1.0 made adjacent glyphs collide
# ("Ballandchain"), and anything fractional lands unevenly once PIL rounds each
# glyph to a pixel ("Gra c e"). The font's own advances are already right.
MAX_FONT_SIZE = 13
MIN_FONT_SIZE = 7
LETTER_SPACING = 0.0

OUTPUT_DIR = os.path.dirname(os.path.abspath(__file__))

# Vertical budget for cap height + descender: the silhouette needs OUTLINE_PX
# above the caps and OUTLINE_PX + DROP[1] below the descenders, inside BORDER.
V_BUDGET = HEIGHT - 2 * BORDER - 2 * OUTLINE_PX - DROP[1]
H_BUDGET = WIDTH - 2 * (BORDER + OUTLINE_PX + H_MARGIN) - DROP[0]

# ---------------------------------------------------------------------------
# All custom items: (filename_base, display_name)
# ---------------------------------------------------------------------------
ALL_ITEMS = [
    ("gRocsFeatherNameTex", "Roc's Feather"),
    ("gRocsCapeNameTex", "Roc's Cape"),
    ("gDesireSensorNameTex", "Desire Sensor HP3"),
    # Stone of Agony level 2 — renames the quest cell it shares with the stone.
    ("gQuartzOfMotionNameTex", "Quartz of Motion"),
    ("gHyliaGraceNameTex", "Hylia's Grace MP24"),
    ("gZonaiPermafrostNameTex", "Zonai Timer"),  # renamed (user 2026-08-06); symbol kept so no code changes
    # 2026-08-06 page-2 additions (regenerate to produce their name textures):
    ("gSheikahSlateNameTex", "Sheikah Slate"),
    ("gPhantomHourglassNameTex", "Phantom Hourglass"),
    ("gShadowCrystalNameTex", "Shadow Crystal"),
    ("gRodOfSeasonsNameTex", "Rod of Seasons"),
    ("gDemiseDestructionNameTex", "Demise's Destruct. MP12"),
    ("gDekuLeafNameTex", "Deku Leaf MP1"),
    ("gSwitchHookNameTex", "Switch Hook"),
    ("gMogmaMittsNameTex", "Mogma Mitts MP1"),
    ("gGustJarNameTex", "Gust Jar"),
    ("gBallAndChainNameTex", "Ball and Chain"),
    ("gWhipNameTex", "Whip"),
    ("gSpinnerNameTex", "Spinner"),
    ("gCaneOfSomariaNameTex", "Cane of Somaria"),
    # Dual Cane — the Pacci chain's own cell name, plus the two level-3 upgrades
    # that become their own entries in the wheel once obtained (Skijer's NEI).
    ("gCaneOfPacciNameTex", "Cane of Pacci"),
    ("gCaneOfByrnaNameTex", "Cane of Byrna"),
    ("gTrirodNameTex", "Trirod"),
    ("gUltrahandNameTex", "Ultrahand"),
    ("gDominionRodNameTex", "Dominion Rod"),
    # Elemental Wand — six rods sharing ONE page-2 cell, so the NAME follows the active mode
    # (ExtInv_GetCustomItemNameTex resolves it from Wand_GetMode). Skijer's NEI
    ("gSandRodNameTex", "Sand Rod"),
    ("gTornadoRodNameTex", "Tornado Rod"),
    ("gWaterRodNameTex", "Water Rod"),
    ("gMeteorRodNameTex", "Meteor Rod"),
    ("gStormRodNameTex", "Storm Rod"),
    ("gShadowScepterNameTex", "Shadow Scepter"),
    ("gTimeGateNameTex", "Time Gate"),
    ("gBombArrowsNameTex", "Bomb Arrows"),
    ("gFireRodNameTex", "Fire Rod MP3"),
    ("gIceRodNameTex", "Ice Rod MP3"),
    ("gLightRodNameTex", "Light Rod MP3"),
    ("gBeetleNameTex", "Beetle"),
    ("gShovelNameTex", "Shovel"),
    ("gMinishCapNameTex", "Minish Cap"),
    ("gLanternNameTex", "Lantern"),
    ("gPokeballNameTex", "Pokeball"),
    ("gIronKnuckleAxeNameTex", "Iron Knuckle Axe"),
    ("gDrillshaftNameTex", "Drillshaft"),
    ("gTridentNameTex", "Trident"),
    ("gFourSwordNameTex", "Four Sword"),
    ("gGerudoScimitarNameTex", "Gerudo Scimitar"),
    # NEI progressive sword upgrade names (Razor/Gilded/Great Fairy) come from mm.o2r
    # (item_name_static/gItemName*SwordENGTex) — no custom textures needed for those.
    ("gSheikahShieldNameTex", "Sheikah Shield"),
    ("gSpiritBreastplateNameTex", "Spirit Breastplate"),
    ("gKiteShieldNameTex", "Kite Shield"),
    ("gMagicArmorNameTex", "Magic Armor"),
    ("gDivineShieldNameTex", "Divine Shield"),
    ("gGoddessShieldNameTex", "Goddess Shield"),
    ("gShieldOfIkanaNameTex", "Shield of Ikana"),
    # Page-2 equipment / tunics + boots.
    ("gChampionsTunicNameTex", "Champion's Tunic"),
    ("gMagicTunicNameTex", "Magic Tunic"),
    ("gSagesTunicNameTex", "Sage's Tunic"),
    ("gMagicCapeNameTex", "Magic Cape"),
    ("gWaterDragonScaleNameTex", "Water Dragon Scale"),
    ("gPegasusBootsNameTex", "Pegasus Boots"),
    ("gPegasusAnkletNameTex", "Pegasus Anklet"),
    ("gRocBootsNameTex", "Roc Boots"),
    ("gClimbBootsNameTex", "Climb Boots"),
    # Placeholders for equipment cells whose real item is not decided yet.
    # SM64 Mario Mode (soh-only asset).
    ("gMarioMaskNameTex", "Mario Mask"),
    # Rito form trigger; shares the Farore's Wind cell.
    ("gRitoMaskNameTex", "Rito Mask"),
    ("gPending2NameTex", "Pending 2"),
    ("gPending3NameTex", "Pending 3"),
    ("gPending4NameTex", "Pending 4"),
    # Twilight Upgrade mode-toggle names (shown when Clawshot/Gale modes are active
    # via the A-button toggle on hookshot/longshot or boomerang).
    ("gClawshotNameTex", "Clawshot"),
    ("gGaleBoomerangNameTex", "Gale Boomerang"),
    # Hookshot overhaul: Longshot L3 (Longshot icon + Light-medallion marker, name reads Ultrashot).
    ("gUltrashotNameTex", "Ultrashot"),
    # Bottle Randomizer extra items (Net + Bottomless Bottle).
    ("gNetNameTex", "Net"),
    ("gBottomlessBottleNameTex", "Bottomless Bottle"),
    # NEI custom ocarina songs. Used by BOTH quest pages: the OoT collect page in
    # 2ship (sOotNamePaths) and the MM collect page in soh (sMmPageSongNames), where
    # they replace the doubled Epona/Time/Storms rows. Keep the two repos in sync.
    ("gFugueOfHomeNameTex", "Fugue of Home"),
    ("gCommandMelodyNameTex", "Command Melody"),
    ("gBalladOfHeroNameTex", "Ballad of Hero"),
]


def find_font():
    """Find Century Gothic Bold font."""
    paths = [
        "C:/Windows/Fonts/GOTHICB.TTF",
        "C:\\Windows\\Fonts\\GOTHICB.TTF",
        os.path.join(OUTPUT_DIR, "..", "..", "fonts", "CenturyGothicBold.ttf"),
    ]
    for p in paths:
        if os.path.exists(p):
            return p
    return None


def font_metrics(font):
    """(ascent, cap_height_above_baseline, descender_depth_below_baseline).

    Taken from the FONT, never from the string being drawn — that per-string bbox
    is exactly what made the old baselines jump around.
    """
    ascent, _descent = font.getmetrics()
    cap_box = font.getbbox("H")
    desc_box = font.getbbox("gjpqy")
    cap_height = ascent - cap_box[1]
    descender = max(0, desc_box[3] - ascent)
    return ascent, cap_height, descender


def pick_base_size(font_path):
    """Largest size whose caps + descenders fit V_BUDGET. Shared by all textures."""
    for size in range(MAX_FONT_SIZE, MIN_FONT_SIZE - 1, -1):
        font = ImageFont.truetype(font_path, size)
        _ascent, cap_height, descender = font_metrics(font)
        if cap_height + descender <= V_BUDGET:
            # Caps start one outline ring below the transparent border.
            baseline = BORDER + OUTLINE_PX + cap_height
            return size, baseline
    raise RuntimeError(f"no font size in [{MIN_FONT_SIZE}..{MAX_FONT_SIZE}] fits {V_BUDGET}px")


def measure(font, text, spacing=LETTER_SPACING):
    """Advance width of `text` with per-character spacing applied."""
    if not text:
        return 0.0
    total = sum(font.getlength(ch) for ch in text)
    return total + spacing * (len(text) - 1)


def fit_size(font_path, text, base_size):
    """Shrink only as far as needed to fit H_BUDGET. Baseline is unaffected."""
    for size in range(base_size, MIN_FONT_SIZE - 1, -1):
        font = ImageFont.truetype(font_path, size)
        if measure(font, text) <= H_BUDGET:
            return font, size
    return ImageFont.truetype(font_path, MIN_FONT_SIZE), MIN_FONT_SIZE


def render_mask(font, text, baseline):
    """Antialiased glyph coverage (mode "L"), glyphs sitting on `baseline`."""
    mask = Image.new("L", (WIDTH, HEIGHT), 0)
    draw = ImageDraw.Draw(mask)

    ascent, _cap, _desc = font_metrics(font)
    width = measure(font, text)
    pen = (WIDTH - width) / 2.0
    top = baseline - ascent  # default "la" anchor draws from the ascender line

    for ch in text:
        # Round only the DRAW position; the pen keeps its fractional advance so
        # cumulative spacing stays true instead of drifting one px per glyph.
        draw.text((round(pen), top), ch, font=font, fill=255)
        pen += font.getlength(ch) + LETTER_SPACING

    return mask


def dilate(mask, radius):
    """Grow a mask by `radius` px in all 8 directions (3x3 max filter, repeated)."""
    out = mask
    for _ in range(radius):
        out = out.filter(ImageFilter.MaxFilter(3))
    return out


def compose_ia4(mask):
    """Build the RGBA image, already quantised to what IA4 can store.

    alpha  -> 2 levels  (silhouette = glyph + outline ring + drop copy)
    RGB    -> 8 levels  (the 3-bit intensity; 0 = ENV navy, 255 = PRIM white)
    """
    solid = mask.point(lambda v: 255 if v >= AA_THRESHOLD else 0)

    silhouette = dilate(solid, OUTLINE_PX)
    if DROP != (0, 0):
        shifted = Image.new("L", (WIDTH, HEIGHT), 0)
        shifted.paste(solid, DROP)
        silhouette = Image.composite(shifted, silhouette, shifted)

    # 3-bit intensity, carrying the antialiasing between outline and letter core.
    step = 255.0 / (IA4_INTENSITY_LEVELS - 1)
    intensity = mask.point(lambda v: int(round(round(v / step) * step)))

    img = Image.merge("RGBA", (intensity, intensity, intensity, silhouette))
    return img


def generate_name_texture(text, font_path, base_size, baseline, output_path):
    font, size = fit_size(font_path, text, base_size)
    mask = render_mask(font, text, baseline)
    compose_ia4(mask).save(output_path)
    return size


# ---------------------------------------------------------------------------
# Verification
# ---------------------------------------------------------------------------
def verify_file(path):
    """Report anything an .ia4.png must not have. Returns a list of problems."""
    img = Image.open(path).convert("RGBA")
    if img.size != (WIDTH, HEIGHT):
        return [f"size {img.size} != ({WIDTH}, {HEIGHT})"]

    px = img.load()
    alphas = set()
    intensities = set()
    min_x, max_x, min_y, max_y = WIDTH, -1, HEIGHT, -1

    for y in range(HEIGHT):
        for x in range(WIDTH):
            r, g, b, a = px[x, y]
            alphas.add(a)
            if a:
                intensities.add((r, g, b))
                min_x = min(min_x, x)
                max_x = max(max_x, x)
                min_y = min(min_y, y)
                max_y = max(max_y, y)

    problems = []
    if not alphas <= {0, 255}:
        problems.append(f"{len(alphas)} alpha levels (IA4 has 2)")
    if len(intensities) > IA4_INTENSITY_LEVELS:
        problems.append(f"{len(intensities)} intensity levels (IA4 has {IA4_INTENSITY_LEVELS})")
    if any(r != g or g != b for r, g, b in intensities):
        problems.append("non-grey texels (IA4 has no chroma)")
    if max_x < 0:
        problems.append("empty texture")
    else:
        if min_x < BORDER or max_x > WIDTH - 1 - BORDER:
            problems.append(f"clipped horizontally (ink x {min_x}..{max_x})")
        if min_y < BORDER or max_y > HEIGHT - 1 - BORDER:
            problems.append(f"clipped vertically (ink y {min_y}..{max_y})")
    return problems


def cmd_verify():
    bad = 0
    for fname in sorted(os.listdir(OUTPUT_DIR)):
        if not fname.endswith(".ia4.png"):
            continue
        problems = verify_file(os.path.join(OUTPUT_DIR, fname))
        if problems:
            bad += 1
            print(f"  [BAD ] {fname}: {'; '.join(problems)}")
        else:
            print(f"  [ OK ] {fname}")
    print()
    print("All textures are IA4-clean." if not bad else f"{bad} texture(s) need regenerating (--all).")
    return bad


def cmd_orphans():
    """.ia4.png files on disk that ALL_ITEMS does not know about."""
    listed = {name for name, _ in ALL_ITEMS}
    disk = {f[: -len(".ia4.png")] for f in os.listdir(OUTPUT_DIR) if f.endswith(".ia4.png")}
    return sorted(disk - listed), sorted(listed - disk)


def main():
    font_path = find_font()
    if not font_path:
        print("ERROR: Century Gothic Bold not found!")
        print("Install it or place CenturyGothicBold.ttf in the fonts folder")
        sys.exit(1)

    if "--verify" in sys.argv:
        sys.exit(1 if cmd_verify() else 0)

    base_size, baseline = pick_base_size(font_path)

    print(f"Font:     {font_path}")
    print(f"Output:   {OUTPUT_DIR}")
    print(f"Format:   IA4 ({IA4_INTENSITY_LEVELS} intensity levels, 2 alpha levels)")
    print(f"Size:     {base_size}px, baseline y={baseline}, outline {OUTLINE_PX}px + drop {DROP}")
    print(f"Budgets:  {H_BUDGET}px wide, {V_BUDGET}px tall")
    print()

    existing = {f[: -len(".ia4.png")] for f in os.listdir(OUTPUT_DIR) if f.endswith(".ia4.png")}

    generate_all = "--all" in sys.argv
    only_missing = "--missing" in sys.argv

    # Single custom item: generate_names.py "FileName" "Display Text"
    if len(sys.argv) >= 3 and not sys.argv[1].startswith("-"):
        name, text = sys.argv[1], sys.argv[2]
        out = os.path.join(OUTPUT_DIR, f"{name}.ia4.png")
        size = generate_name_texture(text, font_path, base_size, baseline, out)
        note = "" if size == base_size else f"  (shrunk to {size}px to fit)"
        print(f'  {name} -> "{text}"  OK{note}')
        if name not in {n for n, _ in ALL_ITEMS}:
            print(f"  NOTE: add ('{name}', '{text}') to ALL_ITEMS so --all regenerates it.")
        return

    if not generate_all and not only_missing:
        for name, display in ALL_ITEMS:
            status = "EXISTS" if name in existing else "MISSING"
            print(f'  [{status}] {name}.ia4.png -> "{display}"')
        orphans, missing = cmd_orphans()
        if orphans:
            print()
            print("  Not in ALL_ITEMS (--all will NOT refresh these):")
            for o in orphans:
                print(f"    {o}.ia4.png")
        print()
        print("Usage:")
        print("  python generate_names.py                          # Show status")
        print("  python generate_names.py --missing                # Generate only missing")
        print("  python generate_names.py --all                    # Regenerate all")
        print("  python generate_names.py --verify                 # Check every PNG is IA4-clean")
        print('  python generate_names.py "gMyItemTex" "My Item"   # Single custom')
        return

    items = [(n, d) for n, d in ALL_ITEMS if generate_all or n not in existing]
    if not items:
        print("Nothing to generate!")
        return

    print(f"Generating {len(items)} textures...")
    shrunk = []
    for name, display in items:
        out = os.path.join(OUTPUT_DIR, f"{name}.ia4.png")
        size = generate_name_texture(display, font_path, base_size, baseline, out)
        if size != base_size:
            shrunk.append((name, display, size))
        print(f'  {name} -> "{display}"' + ("" if size == base_size else f"  [{size}px]"))

    if shrunk:
        print()
        print("Shrunk to fit 128px (shorter display text would keep them at full size):")
        for name, display, size in shrunk:
            print(f'  {size}px  {name}  "{display}"')

    print()
    print("Done. Run with --verify to confirm, then rebuild the .o2r.")


if __name__ == "__main__":
    main()
