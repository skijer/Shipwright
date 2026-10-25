# Command Melody

Implements the existing `nei.mm_song_command_melody` song. No second song item
is registered. Its description and pause text explain the controls with native
button glyphs.

Play **A, A, C-Up, C-Right, C-Left, C-Down** after learning the song. A targeted
registered partner is preferred; otherwise the nearest available partner is
chosen. Uses an independent subcamera, keeps Link standing in his actual
location and returns control on C-Up, interruption, damage or transitions.
It never changes background music or forces a pause-menu state.

## Partner service ABI v1

`FindService("nei.partner.register.v1")` yields `NeiRegisterPartnerV1` from
`partner_control.h`. Each partner registers its key, name, actor lookup,
begin/end callbacks and input callback. Up to eight partners can register;
duplicate keys update their registration. Runtime actor pointers are queried
again rather than retained across scenes.

The provider owns movement, collision, animation and attacks; the song owns
input routing, Link's waiting action, interruption policy and camera. This is
the integration point for Malon, Cojiro and enemy possession. No dependency on
Malon's implementation or a shared helper library is required.

Malon currently supports Stick movement, A jump, B bow attack. C-Left/Right
rotate the camera and C-Up returns to Link. Pause stays available.

The optional appended `flags` field accepts `NEI_PARTNER_TRACK_PLAYER` for
possession. Older providers without that field are zero-filled and retain
stationary-Link companion behavior. Tracking providers hide Link and move his
real position with their actor; exit restores draw, shadow, parent, midair
state and invulnerability. This mode permits aerial control and room loading.

`nei.partner.begin.v1` publishes `bool (*)(const char* providerKey)` for an
item to request control without playing a song. Requests defer while native
text, pause, cutscenes or transitions are active. `nei.partner.end.v1`
publishes `void (*)(void)` to exit through the same restoration path.
