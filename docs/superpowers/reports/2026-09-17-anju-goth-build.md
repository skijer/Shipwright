# Anju goth palette and build preparation

The user approved the R5 geometry and requested a goth palette variant like the Treasure Chest Shop Gal before starting the build. The new archive is an alternative to the approved auburn archive; both use the same Anju HD v1 adapter.

## Palette and preservation

- Glossy black hair and matching brows, charcoal vest/shirt/skirt, black leather heels, silver-gray belt and embroidered trim.
- Native umbrella geometry and monochrome pattern retained; its prim/env colors provide a black canopy, silver ornament and dark handle.
- Three artwork images were edited with the built-in image-generation tool from the approved R5 source sheets. Layout and sheen were constrained to the source. The original face/skin and eye resources remain byte-identical.
- Separate 143 shoe triangles from 75 skin triangles per foot, changing only material assignment. Ankle skin continues to sample the original atlas. The same treatment fixes 34 cloth triangles along the torso/sleeve boundaries that previously sampled the skin atlas's pale-green cloth islands.
- Canonical decoded triangle comparisons for both pose candidates confirm identical R5 positions, UVs, normals, winding, flags, and bone ownership. Existing vertex payloads are untouched; the new shoe/skin lists repack the original rows.
- The archive contains no scene, placement, collision, skeleton, or animation overrides. The approved grip, thumb, sleeves, skirt, vest, heels, anchor and blink geometry are retained.

## Verification before publication

- Goth archive CRC and exact geometry comparisons pass. Only the expected material resources and metadata change.
- Native production graph fixture: 19 limbs, three blink heads, umbrella, 2948 owned vertex loads, complete fallback and eight cache/Alt reentry cycles pass.
- All seven configured static-story CTests pass. Production viewer/asset-loader objects are current.
- Fresh production-function fixtures pass: 22 normal presentations, Kafei skeleton/animation/face loading and public draw dispatch, Anju native metadata isolation, fixed XYZ/yaw and umbrella attachment, HD blink timing, cache/reentry, and existing Skull Kid/Lulu/Shop Gal/Fairy behavior.
- Clang-format 14.0.6 passes for all 14 changed C/C++ files covered by the repository's formatting policy. Python compilation and `git diff --check` pass.
- Offline model previews use the exported archive. No in-game result is claimed.

## Build scope

Publish one bundled commit/branch build from `e4e71eeea6e173a35405aa708507d5afbdea4089`, preserving the earlier Skull Kid/Tael and draw-distance work. Include Kafei's missing draw dispatch, seated crying Anju (`0x7E0D`), dialogue, the isolated native/HD adapter, blink behavior, and the approved R5 asset tooling plus this optional goth palette tooling.

Standing umbrella idle remains a preview candidate, and walking/path following is not added. Model archives are separate downloads and are not embedded into the fork. Choose one of the auburn or goth archives; do not load both.

Goth archive: `Anju_HD_Goth_Umbrella_R5.o2r`, 152 resources, 8,934,876 bytes.

SHA256: `872d92ae63cfdcb6e6970605e23c3693e1ee3fd79f37619fe5acc4f099a7a41c`.
