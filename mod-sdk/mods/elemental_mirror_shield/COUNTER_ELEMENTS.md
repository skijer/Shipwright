Elemental Mirror Shield

The vanilla OoT Mirror Shield returns blocked magic while Link is shielding.
The existing shield and light-puzzle behavior remains available. Custom shields
and transformation forms keep their own behavior. The power is disabled in the
Spirit Temple boss scene and whenever a living Twinrova actor is present.

Counter list, in priority order:

- Freezard: blue fire, using native fire-magic damage.
- A burning Fire Keese: thunder magic, with electrical sparks and real collision damage.
- Anubis fireballs and Flare Dancers: ice magic.
- Ice flames, ice arrows, ice magic and freezing attacks: blue fire.
- Other fire arrows, fire magic and burning attacks: ice magic.
- Electrical attacks: light magic.
- Light arrows, light magic and mirror rays: light magic.
- Physical attacks: ordinary shield behavior; no additional projectile.

Returned bolts aim at the attacker, its enemy parent or Link's current target.
They collide with walls and enemy colliders; they do not directly change HP or
kill an enemy. OoT has no thunder damage bit, so thunder uses the light-magic bit
and supplies two collision damage when the enemy's light table would give zero.
Its electrical effect is visual, without applying an ice damage reaction.

This behavior is active while the mod is enabled; there is no separate item configuration.
