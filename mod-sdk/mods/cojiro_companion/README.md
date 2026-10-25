# Cojiro Companion

`drop custom_item "nei.equip.cojiro"`

A passive companion equipment below the Magic Cape and Pendant. A on its pause
slot calls or dismisses him. Completing Grog's original Cojiro check also grants
this special Cojiro after the ordinary check reward, without replacing that
reward. Existing saves with that check completed are eligible too.

Uses the native cucco skeleton, animation, and native procedural wing/neck
poses in a separate friendly actor (`nei.cojiro_partner`). He follows, hops at obstructions,
recalls to grounded Link when too far away, and respawns across scenes. He can
be carried and thrown. Nearby idle movement alternates short runs, hops and
resting/pecking. He flees briefly when hit, clucks, cries when carried and
uses native wing-flap sounds. He does not run the vanilla revenge swarm AI.

The three white feather textures and the border-connected feather background
of the eye texture are tinted blue in local copies of the native display
lists. The eye's iris, pupil and inner highlights remain native, as do beak,
comb and claw textures. This prevents a white square around the eye. Other
cuccos' shared assets are never patched. Regenerate with
`build_assets.py.txt` against the installed oot.o2r; the report records the seven
limb bindings and material substitutions.

Any attack type can hit him, including Link's attacks. Three distinct hits
with a 16-update invulnerability interval dismiss him for 400 active gameplay
updates. Owned/toggle state is saved; temporary hit and recovery state reset
when loading a file.

Requires `command_melody`. Play A, A, C-Up, C-Right, C-Left, C-Down near him
after learning that song. Target him to choose him over another partner.
Stick moves, A jumps, holding A slows descent, B crows, C-Left/Right rotates
the camera, and C-Up returns to Link. Carrying him also gives Link native
cucco falling settings: gravity -0.5, maximum fall speed -2, and continuous
fall-start reset. These settings apply only while carrying this companion
and descending; native movement restores its own settings on other actions.

Compile/install from Shipwright-unbound with
`./build-all.local.ps1 -Only Mods -Reconfigure`. Logic tests cover the real
three-hit function; following, trade delivery, appearance and direct control
still require in-game feedback.
