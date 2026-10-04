# Child Ruto mouth segment correction

| Field | Record |
| --- | --- |
| Baseline | `d3323797f9c29f422031493a33b44512020c632a`, shared `poc/lost-woods-scene-rain` worktree. Concurrent rain/chest/Shop Gal changes are outside this fix. |
| Candidate | Child Ruto face-binding correction in `EnViewer_DrawStaticChildRuto`. |
| Scope | Bind the neutral mouth to object_ru1's actual dynamic mouth segment, `0x09`. No asset/archive edits. |
| Preservation | Native and Alt symbolic asset lookup, three blink states, geometry/UVs, skeleton, three poses, tracking callback, actor parameters/state, and opaque render mode remain unchanged. Adult Ruto and native story actor code remain unchanged. |
| Configuration | Static child Ruto `0x7F07` (hands behind back), `0x7F17` (hands on hips), `0x7F27` (sitting). Actual Alt pack/load order and pictured renderer are not established. |
| Evidence | Production draw fixture fails before correction and passes afterward for all nine pose/eye combinations. Read-only native head archive check, full viewer C syntax check, catalogue test and adult Ruto water test pass. |
| Verdict | Implemented and CPU/static verified. Full game build, game rendering, Alt pack appearance, Prelude editor preview and user acceptance remain untested. No master promotion. |
| Recovery | Revert only the Child Ruto segment-binding hunk to restore the baseline adapter; no source or candidate archive needs replacement. |

## Confirmed cause

The static adapter bound eyes to both `0x08` and `0x09`, then bound its mouth to
`0x0A`, copying the adult Ruto layout. Child Ruto uses a different contract:
native `EnRu1_DrawOpa` and `EnRu1_DrawXlu` finish with the mouth on `0x09`.
The actual `objects/object_ru1/gRutoChildHeadDL` in the available `oot.o2r`
also requests eye and mouth textures from segments `0x08` and `0x09`.
Consequently, the baseline static actor feeds its eye texture to the mouth
surface. This independently confirms a SoH defect matching the reported duplicate
eyes near the upper lip.

The correction keeps the blink texture on `0x08`, supplies
`gRutoChildMouthClosedTex` on `0x09`, and removes the unused `0x0A` binding.
All texture references remain symbolic, allowing ordinary resource-pack lookup.
No replacement model, texture repaint, blink freeze or geometry alteration is
introduced.

Read-only archive evidence:

- `oot.o2r` SHA-256: `ea80d61b223ee38075fe5383b652998f3b08164e903134922f63f205d995717b`
- Native child head resource: 2,488 bytes; SHA-256 `d167be07c913798b4d7c816731b72cb0bdf58667f6f5c1afab52e4a91ccb328e`
- Texture image commands include `0x08000001` and `0x09000001`; no `0x0A` texture segment.

## Verification

Run the standalone regression without game assets:

```sh
python3 -B scripts/diagnostics/run_child_ruto_face_test.py
```

Optionally supply the original native archive for the independent head contract:

```sh
python3 -B scripts/diagnostics/run_child_ruto_face_test.py /path/to/oot.o2r
```

The fixture compiles the actual production Child Ruto draw function and limb
callback against the real viewer types. At the skeleton draw boundary it reads
the emitted GBI commands and verifies the model receives the correct eye and
mouth resources, even after a preceding actor's segment bindings. It exercises
all three poses and open/half/closed eyes, and checks actor state is unchanged.
The resource manager/GPU is outside this CPU fixture; it does not prove an
installed Alt model's appearance.

Before the fix the fixture failed with:

```text
Child Ruto mouth segment 0x09 resolves to __OTR__objects/object_ru1/gRutoChildEyeOpenTex (eye state 0, pose 0)
```

After the fix, the nine cases and native archive audit pass. The runner also
syntax-checks the complete viewer translation unit. Existing qualifier warnings
and unrelated implicit `memcpy` declaration warnings remain in that check.
`static_story_actor_test`, `static_story_ruto_water_test`, and `git diff --check`
also pass. The dedicated runner can be called from the shared stabilization
runner without a CMake change; it does not require the optional archive.

## Renderer and runtime limits

The supplied screenshot was reported to show `Show fog` and `Clip at draw
distance`, suggesting an editor preview. Its local attachment was unavailable
to this diagnostic, and Prelude source was not available locally or through the
connected repository searches. This change is confined to SoH and is not proof
that Prelude's separate preview renderer is repaired.

The next decisive check is the same static child Ruto placement in the candidate
SoH build, normal and Alt assets, viewed from the front through a blink cycle.
The mouth should stay a mouth while only the eyes blink. If the defect remains
only in Prelude, inspect that preview's object_ru1 segment `0x09` binding with the
same source pack. Leave accepted source archives unchanged.
