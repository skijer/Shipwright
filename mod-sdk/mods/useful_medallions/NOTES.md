# useful_medallions — notas

Port 1:1 de las SW97 Medallion Spells de la rama NEI (`Shipwright-nei/soh/expansions/sw97`). Standalone: no
depende de ningún otro mod. Las piedras espirituales viven ahora en `spiritual_stones/`.

## Qué hay

- `actors/` — los 12 actores SW97 (6 hechizos + 6 flechas), copia verbatim de `modapi-hooks/tools/mods-api/sw97`
  (que ya era 1:1 con NEI). Solo cambian los includes, el derretido de hielo rojo (`Sw97_MeltRedIce`, réplica de
  `BgIceShelter_MeltInstantly` de NEI) y el hechizo de Espíritu, que llama a `fairy_mode.c`.
- `spells.c` — los medallones como items de la pantalla de quest. El lanzamiento es una action func propia que
  replica `func_8083AF44` + `Player_Action_808507F4` de NEI con las tablas de 6 hechizos (animación, cámara,
  voz, `MAGIC_CONSUME_WAIT_PREVIEW`). Trueque de Sombra (mantener C: −3 corazones, +24 de magia).
- `arrows.c` — elemento cebado por arco y por tirachinas (ModStorage `elements`). El arco va por
  `OnPlayerResolveItemAction` a `PLAYER_IA_BOW_FIRE..0E` como `ExtPlayer_GetItemAction` de NEI; el hijo vanilla de
  la flecha se sustituye por el actor SW97. Robo de vida de la flecha oscura, muros de barro con hielo y
  interruptores de sol con luz (lo que NEI enciende con SW97 activo).
- `arrow_ui.c` — icono compuesto en el botón (medallón al 50 % + arma al 75 %, pregenerado en `assets/`), selector
  con A en la celda del arco o del tirachinas, y rueda al mantener C.
- `fairy_mode.c` — el estado "Ivan" de Hylia's Grace: EnPartner controlado por el jugador 1 y noclip.

## Diferencias con NEI que impone Unbound

- **Tipos de flecha:** NEI añadió `ARROW_SW97_*` (17-22) a `z_en_arrow`. Aquí las flechas usan los tipos vanilla
  `ARROW_FIRE..ARROW_0E`, que tienen los mismos `dmgFlags`; el mod pone la estela de 0C-0E, el sonido de disparo
  y anula el coste de magia (en NEI las flechas SW97 son gratis).
- **Semillas elementales:** siguen siendo `ARROW_SEED`; el mod les da los `dmgFlags` del elemento y el actor SW97.
- **Modo hada:** NEI usa `gIvanPossessActive` en el motor; aquí el mod toma prestadas las CVars
  `gEnhancements.IvanCoopModeEnabled` y `gCheats.NoClip` mientras dura, y las devuelve al salir o al cambiar de
  escena. Si la config se guarda en pleno vuelo, se guardarían encendidas.
- **Sin Bomb Arrows en la rueda:** son otro mod; meterlas rompería el standalone.
- **Sin ciclo con R/L al apuntar:** en NEI es una opción aparte (`NeiAimCycle`, apagada por defecto).

## Costes (los del código de NEI, no los de su texto de pausa)

Forest 12 · Spirit 24 · Shadow 24 · Water 12 · Light 24 · Fire 12. El texto de NEI dice Water 24 y Shadow 12,
pero `sMagicSpellCosts` cobra lo de arriba.

## Pendiente

Reconfigurar el SDK (entradas nuevas en `tools/mods-api/CMakeLists.txt`), compilar y probar en juego.
