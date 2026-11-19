# Anim Viewer

Open **Dev Tools → Anim Viewer** and press **Refresh**.

- Search by name or path; presets find vaults, spins and slashes.
- Pick the archive in **Source** and a clip from the list. **Play on Link**, **Pause / Resume**, **Stop** and **Restart clip** control playback.
- **Frame** scrubs to a pose, **Speed** covers slow motion and reverse, **Loop** repeats the clip.
- **Lock root translation** keeps Link in place during the preview.
- **Copy animation path / Copy raw clip path** copy the paths to the clipboard.
- **Mark for Byrna** saves the clip and frame; **Export candidates** writes them to `mods/animviewer_byrna.txt`.

Start a preview with Link on safe ground, outside dialogue and cutscenes. **Stop** returns control. The inventory is untouched.

## Other archives

**Import O2R** copies an archive into `mods`; restart and press **Refresh**. An archive that contains mods loads them too. **Include resources with custom names** also lists clips without "anim" in their path.

Clips must target Link's 67-value human skeleton. NPC and form clips need their own skeleton.
