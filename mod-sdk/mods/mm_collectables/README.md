# MM collectables: pause support

The MM quest page previews an owned MM song after the cursor rests on it briefly.
Its button sequence appears on the staff. Moving away or closing pause cancels
the preview. Preview playback does not execute song effects.

Press **A** on an owned MM song to close pause normally and play the sequence.
After playback, the mod delivers the notes through the existing recognition hook,
including listeners from other mods such as Command Melody. This uses the song
effects already implemented by the installed mods; it does not add new effects
to songs that do not have one yet.

Cursor corners follow the actual icon quad, including edited layout positions
and sizes. Song icons use the mod's existing RGBA32 assets rather than MM's IA8
note texture, because the layout draws custom icons as RGBA32.

Playback uses mod-owned note data and the exported native ocarina player. It
does not overwrite vanilla song tables or the save's scarecrow songs. Scene,
file, pause, transition and game-over changes cancel pending playback.

## Manual verification

1. Restart the game to load `mm_collectables` 2.11.
2. Open the MM quest page and move between all four remains, both song rows,
   and the shared vanilla items. Check cursor alignment and clean song icons.
3. Hover an owned MM song. Check that its complete button sequence appears and
   that it plays once. Moving away during preview must stop it.
4. Press A on Elegy, Soaring or Fugue. Pause must close, the melody must play,
   and the existing effect or destination prompt must run only after playback.
5. Test Command Melody with its mod enabled and a nearby supported companion.
6. Reopen pause during autoplay, switch pages during preview, and leave the scene.
   Check that no song effect fires after cancellation and that BGM volume returns.
7. Check unowned songs, an ordinary OoT quest page and repeated A presses.

Build and install from `Shipwright-unbound` using
`./build-all.local.ps1 -Only Mods -Reconfigure`.

## MM-only page and NEI parity

This mod registers the seven MM-exclusive songs plus Fugue of Home and Ballad
of Hero. Saria and Sun remain on the native OoT page.
Songs owned by other mods join the page themselves: they place their item on the
`nei.mm_collectables` layout page and call `FindService("nei.mm_collectables.add_song")`
(`mm_song_page.h`) for the staff preview and A-to-play. Command Melody does this;
its item, recognition and effect stay in `command_melody`.
The preview accepts both native and wide vertex commands, so its cursor/staff
insertion works with the current vertex format. The two custom songs use the
NEI branch fingering and its original playback notes, including ornaments/cycles.

## Song of Soaring in Fleet

Inside Fleet, Soaring shows MM's owl warp map over OoT, drawn with MM's own textures. It shows the statues MM has
activated and the clouds it has cleared, both read from MM's save. Move with the stick or the D-Pad, press A to
choose (`Soar to ...? Yes/No`), or press B or Start to close. Choosing switches to MM, and Link flies in at that
statue. With no statue activated, MM's own refusal text appears in OoT. Outside Fleet the song keeps its list of
OoT owl spots.
