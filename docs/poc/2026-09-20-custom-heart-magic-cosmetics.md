# Custom heart and magic-jar Cosmetic Editor compatibility

Cor requested the custom heart piece/container and magic pot color fix, and
supplied the working recovery-heart POC5 as the reference. The goal is to honor
Cosmetic Editor, not bake a fixed replacement palette into the assets.

## Baseline and cause

Base fork: `c433b6c7e0839d0bd8dddfd9b0de0bb228de76bd`. The preceding rain,
Time Gate chest and child Ruto bundle is retained unchanged.

The actual LA heart and TP magic-jar roots/materials are XML display lists.
The resource loader correctly classifies XML lists as custom. Cosmetic Editor's
heart-piece/container patches target native indices 2/6; magic jars use 31/32.
`ResourceMgr_PatchGfxByName` intentionally skips custom lists because those
positions do not identify the same commands in replacement models.

Recovery hearts already use a different path: `GetItem_DrawRecoveryHeart`
sets the live Cosmetic Editor grayscale tint around the whole model draw.
The supplied POC5 is byte-identical to the recovered reference:
`ee75028b923b92220b6cd703575921b3565bdc322576950553ecd003eb100b44`.

## Candidate

`poc/heart-magic-cosmetics` adds one shared draw helper for custom hearts and
magic jars. It reads the existing `Consumable.Hearts` or `Consumable.Magic`
value when that option is changed, sets the tint, submits the model and disables
the tint afterward. Changes and rainbow updates are read live; reset bypasses
the tint and retains the original material. Native models retain their existing
offset-based behavior. The custom-asset patch guard is unchanged.

Coverage includes held/3D-drop heart pieces and containers, both magic-jar sizes,
placed heart interiors, both opaque/translucent boss-container paths, and the
randomizer's Double Defense container. The separate heart border/exterior draws
stay outside the tint, including Double Defense's white border. Shapes, UVs, textures,
animation, collectible flags, rewards and progression are untouched.

The helper resolves the display list through the existing graphics bridge before
checking whether it is custom. This covers the first Alt draw when the resource
cache still contains a native list. Cache behavior and the bridge's PAK selection
are unchanged; custom classification here applies to the inspected OTR/XML packs.

This is a fork change; existing packs are retained. No new asset archive or
load-order change is required. The LA pack's inner glass shares its heart-piece
body draw, and the TP jar uses one textured body draw: their internal detailing
receives the same tint. This does not promise independent trim-color controls.
Sprite drops with 3D drops disabled are outside the custom 3D-model correction.

## Verification and limits

`python3 scripts/diagnostics/run_custom_item_color_tests.py` executes the actual
production draw functions using real engine types and GBI macros. Resource
classification, CVar storage, matrix allocation and final asset/device rendering
are fixture boundaries. Before the change, 24 required tint cases fail while
24 native/reset controls pass. Review added Double Defense and first-Alt cases;
five of those failed before the two narrow follow-up fixes. All 56 now pass,
including a second live palette, independent heart/magic settings, no tint on
the following bomb, unchanged separate borders and final grayscale disabled.

The runner also compiles all three complete affected C translation units with
implicit function declarations treated as errors, plus the actual Double Defense
body as C++ with the shared C declaration. Existing warnings in the
unrelated generic music-note color array remain; they are not changed here.
The test is included in the existing stabilization runner.
The configured stabilization runner passes: 14 standalone binaries, 10 archive
audit cases, and the audio, night-combat, rain, Time Gate chest, child Ruto and
custom-item integration fixtures. Independent review reproduced the final
56-case and syntax-check results with no remaining actionable findings.

These checks establish emitted draw state, not a launched game's appearance.
The recovery-heart draw routine and supplied POC5 are unchanged. Grayscale
tint retains texture/lighting brightness, so the reported dark shading remains
a separate visual/material question; a selected hex is not a flat unlit color.

Runtime check: use the same LA/TP packs and POC5 with Alt Assets enabled, change
Hearts and Magic to visibly different colors, inspect both jar sizes and a
heart piece/container, then reset. Check a boss container by its warp and a
neighboring ordinary item for tint leakage. Toggle Alt Assets to confirm native
behavior. A full build and configuration-specific visual acceptance must be
recorded separately before treating this as proven.

## Source asset identities

| Source | SHA-256 |
|---|---|
| `zzz1LA Heart Pieces.otr` | `79700a21fd455940f80b9c6163f2914fa5e9f52b83207bc45da0204a0d2c84f0` |
| `zTPMiscObjects.otr` | `31eed67c4255fe389fb5dc39904af3a0b2773f59ad9e8690518a552bb282955f` |
| `zTPMiscObjectsTextures.otr` | `d9e325eb7f01823cb0cf1e8f4f141c5d9a1a204239bef249a735390f7ec1507c` |
| `TP_3D_RecoveryHeart_HexColor_POC5_FullyNeutralBody(1).o2r` | `ee75028b923b92220b6cd703575921b3565bdc322576950553ecd003eb100b44` |

All four originals are unchanged. No asset master has been promoted.
