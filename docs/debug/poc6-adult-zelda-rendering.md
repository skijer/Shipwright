# POC6 Adult Zelda Rendering Diagnosis

## Root cause

The static `En_Viewer` path loaded only `OBJECT_ZL2`, initialized
`gZelda2Skel` without an animation, and skipped animation updates. Native Adult
Zelda (`En_Zl3`) uses `OBJECT_ZL2` for her skeleton/model and
`OBJECT_ZL2_ANIME2` for animation. It waits for the animation bank, binds
segment 6 to that bank, and initializes a real animation before drawing the
15-limb flex skeleton.

The incomplete joint-table pose can explain both the missing body and isolated
yellow geometry observed in POC4. The repair preserves the existing OPA flex
draw and texture segments, requests the missing animation object, binds its
segment, and starts native standing loop `gZelda2Anime2Anim_009FBC`.

## Contract comparison

| Contract | Previous static path | Native / repaired path |
|---|---|---|
| Model object | `OBJECT_ZL2` | `OBJECT_ZL2` |
| Animation object | Missing | `OBJECT_ZL2_ANIME2` |
| Skeleton | `gZelda2Skel` | `gZelda2Skel` |
| Joint pose | No animation initialized | Native standing animation initialized |
| Draw route | OPA `SkelAnime_DrawFlex` | OPA `SkelAnime_DrawFlex` |
| Eye/mouth segments | `0x08`–`0x0A` | `0x08`–`0x0A` |
| Environment segment | `0x0B` | `0x0B` |

No native Zelda action, cutscene, reward, or story-state function is called.
