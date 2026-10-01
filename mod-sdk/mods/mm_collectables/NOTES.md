# mm_collectables — notas

Mod de Unbound con los coleccionables de Majora's Mask: los cuatro restos de jefe (Odolwa, Goht,
Gyorg, Twinmold) como piezas equipables que se llevan en la cara, y las diez canciones que la ocarina
de OoT aprende a reconocer (las 7 de MM + las 3 nuevas de la spec de NEI).

Fuente de verdad: `Shipwright-nei/soh/mods/boss_remains/boss_remains.cpp` (1.936 líneas),
`boss_remains.h`, `boss_remains_actor_reg.cpp` y `Shipwright-nei/soh/mods/mm_songs.cpp`.

## Alta en CMake (la hace el orquestador)

```cmake
unbound_add_mod(mm_collectables
    DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/mm_collectables"
    SOURCES mm_collectables/mm_collectables.c
)
target_link_libraries(mm_collectables PRIVATE z64items)
```

## Qué hace hoy

Los 14 items se registran en `CustomItemRegistry`, página **4**: restos en los slots 0-3, canciones en
los slots 6-15.

**Restos.** Item INSTANT equipable a botón C o D-pad; al pulsarlo se pone o se quita de la cara. La
geometría es la máscara del niño de la luna (`objects/mm_object_ob/gMoonChild*MaskDL`), que es la única
de MM ya ajustada a una cara; se dibuja en `OnPlayerPostLimbDraw` sobre el nodo de la cabeza con la
transformación medida en 2ship, y se protege con `HasResource` (sin mm.o2r el resto sigue siendo
equipable, simplemente no se ve). Una máscara nativa y un resto se excluyen mutuamente.

Efectos mientras se lleva puesto, todos con hooks que ya existen:

| Resto | Efecto | Hook |
| --- | --- | --- |
| Odolwa | A mantenida = carrera ×2, pisadas; el rodar deja de existir | `OnPlayerResolveMotionScale`, `OnPlayerActionHandler` (id `SOH_PLAYER_ACTION_ROLL`) |
| Goht | A mantenida en el suelo = embestida ×3, 1 de magia cada 15 frames, sin daño de caída | `OnPlayerResolveMotionScale`, `VB_RECIEVE_FALL_DAMAGE` |
| Gyorg | Aire infinito bajo el agua y nado ×1,6 | `OnPlayerUpdate`, `OnPlayerResolveMotionScale` |
| Twinmold | La espada hace el doble de daño | `OnResolveSwordDamage` |

**Canciones.** `OnOcarinaNote` alimenta un buffer rodante de 8 notas y lo compara con las secuencias de
las canciones **que ya se poseen**; al acertar suena la campanita de confirmación, igual que en NEI.
Una diferencia deliberada con `mm_songs.cpp`: el hook también dispara al SOLTAR la nota (valor
`OCARINA_NOTE_INVALID`), y NEI mete ese valor en el buffer. Aquí se descarta, que es justo lo que
permite reconocer melodías con dos notas iguales seguidas.

## Assets

Todo lo que el mod necesita viaja dentro de él salvo las cuatro máscaras, que son de mm.o2r.

- `assets/textures/icon_item_custom/` — 14 iconos 32×32 RGBA32 generados (máscara de jefe por color,
  nota musical sobre disco para las canciones). **Son marcadores de posición**: los iconos buenos son
  los de MM (`icon_item_static_yar/gItemIcon*RemainsTex`), y hoy el extractor no los saca (petición 3).
- `assets/textures/item_name_custom/` — 14 texturas de nombre 128×16 IA4. Tres (`gFugueOfHomeNameTex`,
  `gBalladOfHeroNameTex`, `gCommandMelodyNameTex`) salen **verbatim** del soh.o2r de NEI; las once
  restantes se generaron con el `generate_names.py` que ese mismo soh.o2r trae dentro
  (`textures/item_name_custom/generate_names.py`, Century Gothic Bold, verificadas con su `--verify`).
- Referenciado por ruta, no empaquetado: `objects/mm_object_ob/gMoonChild{Odolwas,Gohts,Gyorgs,Twinmolds}MaskDL`.
  No se declara en `requires` a propósito: el mod tiene que cargar igual sin Majora's Mask instalado.

Lista de recursos que hacen falta del extractor de MM y hoy no llegan:

1. `icon_item_static_yar/gItemIcon{Odolwas,Gohts,Gyorgs,Twinmolds}RemainsTex` — los iconos reales.
2. `item_name_static/gItemName{Odolwas,Gohts,Gyorgs,Twinmolds}RemainsENGTex` — los nombres reales.
3. Las secuencias de las canciones de MM (Song of Healing, Soaring, Sonata…) resolubles por ruta.

## Qué quedó fuera, y por qué

La fase 3 de NEI (los movesets completos) **no se ha portado**. No es una decisión de alcance sino de
superficie: en NEI vive dentro de `z_player.c` y de cuatro actores propios, y el ModApi no expone los
puntos donde engancha.

- **Aliados invocados** (`remains_ally_bug/chu/fish/link`): el mod tendría que traerse los cuatro
  actores con `RegisterActor`. Es portable, pero son ~1.200 líneas y cuatro modelos; queda para una
  segunda fase del mod, no para una petición al host.
- **Rayo cargado de Goht, torbellino de Gyorg, polilla de la espada de Odolwa**: dependen de disparar
  en el *inicio del espadazo* y de invocar actores. `OnResolveSwordDamage` llega en la colisión, que ya
  es tarde (petición 4).
- **Vuelo en la nube de polillas de Odolwa y buceo libre tipo Zora de Gyorg**: son máquinas de estado
  que sustituyen el movimiento del jugador frame a frame. `OnPlayerUpdate` corre al final de
  `Player_Update`, después del action func, así que sí serviría para pisar velocidad y posición; lo que
  falta es poder sustituir la animación del cuerpo (petición 5).
- **Goht desenfunda la espada** (en NEI le quita el item del botón B y el `EQUIP_TYPE_SWORD`): se puede
  hacer desde un mod escribiendo `gSaveContext`, pero es exactamente el tipo de manipulación del save
  que deja la partida rota si el juego se cierra con el resto puesto. Sin un hook de "este botón está
  ocupado" no merece la pena.
- **Compañero Dark Link de Twinmold**: tampoco estaba portado en NEI.
- **Las canciones no tienen efecto de juego.** Es la decisión del usuario que ya está tomada en
  `mm_songs.cpp` ("NO gameplay effect yet"); aquí se respeta.
- **Los 14 items no entran en el pool del randomizer.** Meter 14 items nuevos sin reglas de lógica los
  dejaría colocados en cualquier sitio. Hoy se consiguen con `give custom_item "nei.mm_remains_odolwa"`
  o desde el save editor. La forma que tendría, cuando se decida, es un `SOHCustomItemRandomizer` por
  item (`SOH_CUSTOM_ITEM_TYPE_ITEM` para los restos, `SOH_CUSTOM_ITEM_TYPE_SONG` para las canciones,
  flag `ADVANCEMENT`, `poolCount` 0) más las reglas de `RANDO_LOGIC.md`.
- **Las canciones se ven como items de la página 4, no en la página de quest.** En NEI están en la
  página de quest, junto a las de OoT. Desde un mod no se puede (petición 1).

## Las tres canciones nuevas: qué falta decidir

`nei-source.ts` las marca `planned` y no dice nada más. Están registradas, con icono, nombre y textos,
y el reconocimiento funciona; lo que falta es de ti:

1. **La melodía.** No existe en ningún sitio. Las que lleva el mod son **provisionales**, elegidas para
   no chocar con ninguna de las 12 de OoT ni con las 7 de MM:
   - Fugue of Home — C-izq, C-izq, C-abajo, C-arriba, C-der, A
   - Ballad of Hero — C-arriba, C-arriba, C-der, C-abajo, C-izq, A
   - Command Melody — A, A, C-arriba, C-der, C-izq, C-abajo
2. **El efecto.** Los nombres apuntan a algo (Fugue of Home = volver a un sitio fijo; Ballad of Hero =
   algo sobre el héroe; Command Melody = mandar sobre otra cosa, como la de Wind Waker), pero deducirlo
   sería inventarlo. Hoy las tres hacen lo mismo que las siete de MM: sonar y ser reconocidas.
3. **Dónde se consiguen** (mismo problema que el resto: ver el punto del randomizer más arriba).

## Peticiones al host

1. **Hook de dibujo/entrada de la página de quest del kaleido.** El de items existe entero
   (`GameInteractor_HookTable.h:111-118`: `OnKaleidoItemDraw`, `OnKaleidoResolveItemIcon`,
   `OnKaleidoItemCursor`, `OnKaleidoInput`), pero la página de quest no tiene ninguno. El mod pondría
   ahí las diez canciones al lado de las de OoT, que es donde NEI las tiene
   (`Shipwright-nei/soh/src/overlays/misc/ovl_kaleido_scope/z_kaleido_collect.c`,
   `KaleidoScope_DrawMmQuestStatus`). Iría en
   `soh/src/overlays/misc/ovl_kaleido_scope/z_kaleido_collect.c:10`, en `KaleidoScope_DrawQuestStatus`.

2. **Secuencias de archivo resolubles por ruta.** `Unbound_SequenceIdForPath`
   (`soh/soh/unbound/UnboundAudio.cpp:14-36`) sólo devuelve id para lo que `AudioLoad_Init` numeró, y
   eso es el `custom/music/*` de los paquetes. Las secuencias de MM salen de la ROM del usuario a
   `audio/sequences/*` dentro de mm.o2r (`mm_assets/assets/mm_zapd/assets/xml/GC_US/audio/Audio.xml`
   las declara), donde chocan de nombre con las de OoT y no reciben id propio. Sin esto, el mod no
   puede sonar la Song of Healing con la melodía de MM: hoy sólo suena la campanita de confirmación.
   Con ello, `Audio_QueueCustomSeqCmd(playerIdx, fade, id)` ya vale tal cual.

3. **Que el extractor de MM no se salte `interface`.** `mm_assets/sync_zapd_assets.py:19`
   (`SKIPPED = ("interface", "archives")`) deja fuera `icon_item_static_yar` y `item_name_static`, que
   son los iconos y los nombres de verdad de los cuatro restos. El mod trae marcadores generados
   mientras tanto. Si el problema es que ZAPD revienta con `interface` entero (lo dice el README del
   SDK), bastaría con esos dos ficheros sueltos.

4. **Saber que el espadazo ha empezado, no sólo que se ha intentado.**
   `OnPlayerActionHandler` con id `SOH_PLAYER_ACTION_MELEE` ya corre en el momento correcto, pero corre
   ANTES del handler de vanilla (`z_player.c:4251`, `Player_RunActionHandler`): el mod puede sustituir
   el ataque, no enterarse de que ha salido. Odolwa dispara su polilla al *lanzar* el espadazo, tenga o
   no blanco, y `OnResolveSwordDamage` (`soh/src/code/z_collision_check.c:3661`) llega cuando la espada
   ya ha tocado algo. Bastaría con volver a ejecutar el hook tras el handler vanilla con su resultado, o
   un `OnPlayerMeleeAttackStarted(PlayState*, Player*)` en el mismo sitio.

5. **Poder fijar la animación del cuerpo desde un mod.** `OnPlayerResolveAnim(group, animType, anim)`
   sustituye una animación de la tabla, no "reproduce ESTA animación ahora". El vuelo de Odolwa y la
   embestida de Goht en NEI llevan la pose a mano cada frame desde `Player_UpdateCommon`. Un
   `OnPlayerResolveCurrentAnim(Player*, LinkAnimationHeader** anim, f32* frame)` justo antes del
   `LinkAnimation_Update` del jugador cubriría los dos casos y también el de `custom_forms`.

6. **Un dueño para los botones del jugador.** Goht tiene que dejar libre el botón B (en OoT el B se
   reescribe cada frame desde `EQUIP_TYPE_SWORD`, así que sólo se consigue desenfundando de verdad y
   guardando el estado). `BlockPlayerInput(owner, buttons, blockStick)` bloquea la entrada pero no
   vacía el botón ni recupera el item si el juego se cierra. Haría falta algo como
   `ClaimButton(owner, button)` / `ReleaseButton(owner)` que el host restaure al cargar partida.
