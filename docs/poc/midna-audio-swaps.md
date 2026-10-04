# Interchangeable Midna sound cues

Midna's private audio loader reads WAV resources from the active `.o2r` at audio
initialization. Replace a clip in the archive, close the game, install that pack
in place of the previous Midna pack, and restart. An executable rebuild is not
needed for clip substitutions. This also works with the ComboShip Midna port;
install in the relevant game's mods folder and enable that game's Midna option.

All paths below begin with `objects/midna_navi/audio/`.

| Filename | Cue |
| --- | --- |
| `vanish.wav` | Recall |
| `appear.wav` | Emergence |
| `dash.wav` | Fairy dash |
| `target_npc.wav` | NPC targeting |
| `target_enemy.wav` | Enemy targeting |
| `target_other.wav` | Other targeting |
| `call.wav` | Call for attention |
| `hint.wav` | Hint |
| `talk.wav` | Dialogue |
| `yawn.wav` | POC3 idle yawn |

Use ordinary RIFF/WAVE, signed 16-bit little-endian PCM, mono, 32,000 Hz, at most
10 seconds, with nonempty sample data and valid RIFF/chunk lengths. Keep the
exact resource path. A valid silent WAV mutes a cue; missing or invalid ordinary
cues use the existing native fallback. The optional idle yawn has no fallback.
The loader does not hot-reload these clips during play.

The laugh-recall candidate copies the existing enemy-target laugh into
`vanish.wav`. It is the supplied Laugh3 clip, 0.7686875 seconds. No event code,
timing, model, blink, body UV, or other audio resource changes.

| Archive | SHA-256 |
| --- | --- |
| Original `Midna_Navi_SoH_POC3_Model_Audio.o2r` | `dbe4552da4110b845c00a78ea9b02dcc8239ec18214166120de46ddfc2f8c176` |
| `Midna_Companion_POC3_Laugh_Recall.o2r` | `b3621ab7d12a4fa1dec19cca0e967ed95c7a1d12d2fb86e3eb6b78913d23e99f` |

Both archives contain the same 91 resource names and pass ZIP CRC checks. Only
`vanish.wav` differs; the other 90 resources are byte-identical. Replace the
active complete Midna pack rather than enabling two competing complete packs.

Swapping audio content does not change when an event occurs. Movement spacing,
idle-yawn eligibility and interval, or assigning a new gameplay event require
code changes. The existing POC3 movement gap and idle scheduler are preserved.
