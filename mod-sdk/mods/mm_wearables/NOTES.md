# mm_wearables — notas

Las 17 máscaras de Majora's Mask que registra este mod. Las otras siete están en mods propios:
Deku, Goron, Zora, Fierce Deity, Keaton, Kafei y Garo.

## Alta en CMakeLists.txt (la hace el orquestador, no este mod)

```cmake
unbound_add_mod(mm_wearables
    DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/mm_wearables"
    SOURCES mm_wearables/mm_wearables.c mm_wearables/circus_tax.cpp
)
target_link_libraries(mm_wearables PRIVATE z64items)
```

## Rejilla de la página 3

Los slots son los de `gPage3MaskItems[24]` de NEI. Este mod ocupa 17; los otros siete mods
completan la rejilla:

| Slot | Máscara | Quién la registra |
|---|---|---|
| 0 | Postman's Hat | mm_wearables |
| 1 | All-Night Mask | mm_wearables |
| 2 | Blast Mask | mm_wearables |
| 3 | Stone Mask | mm_wearables |
| 4 | Great Fairy's Mask | mm_wearables |
| 5 | Deku Mask | mod `deku` |
| 6 | Keaton Mask | mod `keaton` |
| 7 | Bremen Mask | mm_wearables |
| 8 | Bunny Hood | mm_wearables |
| 9 | Don Gero's Mask | mm_wearables |
| 10 | Mask of Scents | mm_wearables |
| 11 | Goron Mask | mod `goron` |
| 12 | Romani's Mask | mm_wearables |
| 13 | Circus Leader's Mask | mm_wearables |
| 14 | Kafei's Mask | mod `kafei` |
| 15 | Couple's Mask | mm_wearables |
| 16 | Mask of Truth | mm_wearables |
| 17 | Zora Mask | mod `zora` |
| 18 | Kamaro's Mask | mm_wearables |
| 19 | Gibdo Mask | mm_wearables |
| 20 | Garo's Mask | mod `garo` |
| 21 | Captain's Hat | mm_wearables |
| 22 | Giant's Mask | mm_wearables |
| 23 | Fierce Deity Mask | mod `fierce_deity` |

Rito comparte la celda 7 con Bremen; Gerudo y Shadow Crystal comparten la 21 con Captain's Hat.
El orden de la rueda deja primero la máscara original de MM.

## Decisiones

- **Ninguna máscara usa `replacesItem`.** La Bunny Hood y la Mask of Truth de MM conviven con las de OoT en
  vez de sustituirlas, para no romper la cadena de la tienda de máscaras. Es el mismo reparto que en NEI
  distinguía `MmMaskWear_GetOotMaskSource`.
- **Bunny Hood y Mask of Truth prestan el slot vanilla** (`player->currentMask`) en vez de dibujar su DL de
  MM: así funcionan de balde las 54 comprobaciones de NPC que preguntan por `Player_GetMask`, y el modelo es
  el de OoT (el mismo precedente que la máscara del mod `keaton`). Para que vanilla no lo borre cada frame,
  el mod **toma prestada la CVar `gEnhancements.PersistentMasks`** y la devuelve al quitarse la máscara
  (patrón `mogma_mitts`). Ver petición 1: esto debería ser un hook.
- **Sigilo por el hook nuevo `OnActorResolvePlayerRelation`**, no por lo que hacía NEI. Además de falsear
  las dos distancias gira `yawTowardsPlayer` 0x8000, porque `Actor_IsFacingPlayer` mira el rumbo y no la
  distancia. Lo usan la Stone Mask (enemigos, NPC y hostiles), la Gibdo (`ACTOR_EN_RD`) y la Captain's
  (`ACTOR_EN_SKB` / `ACTOR_EN_TEST`).
- **`OnActorPlaySfx`** con los kinds `GENERIC` y `FLAGGED` calla a todo enemigo que no encuentra a Link: un
  enemigo ciego no tiene nada que gritar.
- **Garo's Mask → `VB_GERUDOS_BE_FRIENDLY`.** En MM la máscara es la cara de un espía; OoT no tiene Garos,
  así que su traducción es que las guardias gerudo no te encierren. Es una decisión, no una copia de NEI.
- **El fogonazo de la Giant's Mask usa `play->envCtx.fillScreen`**, el relleno del propio motor, no un quad
  sobre la interfaz: así se blanquea la escena y el HUD juntos, como en cualquier otra transformación.
- **Romani's Mask escribe `DREG(53)`**, que es la bandera que la propia vaca vigila para dar leche tras la
  Canción de Epona. Así la vaca corre su máquina de estados entera, incluido `VB_GIVE_ITEM_FROM_COW` del
  randomizer, sin que el mod la toque. Ver petición 6.

## Uso en interiores y controles

Las máscaras tienen `SOH_CUSTOM_ITEM_WEARABLE`: la restricción de interiores las trata como los objetos
vanilla de intercambio. Esta propiedad no cambia la forma de la definición ni añade un hook.
Los viajes abren el mapa del mundo de la pausa como menú de warp (`mask_travel.inc.c`), el mismo patrón que
`minish_cap`: `RequestTimeControl` + `BlockPlayerInput` + dibujo en `OnInterfaceDrawEnd`, y `VB_OPEN_PAUSE_MENU`
para que START no abra la pausa encima. Stick = destino, A = viajar, B/START = cancelar. Todo son texturas
vanilla por ruta salvo el cursor: la Letter to Kafei (buzón) y la Stray Fairy (fuente) de mm.o2r; si faltan, solo
se ve la caja parpadeando.

## Estado por máscara

| Máscara | Estado |
|---|---|
| Postman's Hat | Buzones propios en las siete escenas de NEI. Se crean al poseer el sombrero; acercarse los descubre y lo guarda en la partida. A junto al buzón o B llevando el sombrero abre el mapa de buzones; se aterriza junto al buzón. |
| All-Night Mask | Completa: spawnea las GS nocturnas de día y mantiene abierta `En_Sw`. Ver petición 4. |
| Blast Mask | Completa: bomba instantánea con %B, 310 frames de recarga y el crossfade de dos DLs de MM. |
| Stone Mask | Completa (sigilo + silencio). |
| Great Fairy's Mask | Física de los tres mechones de MM, corregida para el orden de ángulos de OoT. A invoca a la Gran Hada en la fuente; B abre el mapa de las seis fuentes (abiertas las que ya dieron su recompensa); se aterriza en el pedestal, con fundido blanco. |
| Bremen Mask | Completa: marcha a 3,5 con la animación de MM, cucco a los 120 frames con 400 de recarga. |
| Bunny Hood | Completa: presta el slot vanilla + 1,5× de velocidad (no se suma al enhancement si ya está puesto). |
| Don Gero's Mask | Completa: cobra de golpe todos los premios del coro en el tronco de Zora's River. |
| Mask of Scents | **Parcial**: la animación de olfateo de MM en reposo. Falta el SFX de gruñido (`NA_SE_VO_LI_POO_WAIT` vive en el Soundfont_0 de mm.o2r y no hay puente de audio MM en Unbound) y los puntos de setas, que en NEI son un actor propio. |
| Romani's Mask | Completa: la vaca da leche con A sin canción. |
| Circus Leader's Mask | Interacciones de cobrador de NEI con tiro, bolos, Ingo, Talon, Malon, arquería, pesca, cofres y buceo. Las recompensas se entregan antes de marcar sus flags, también en randomizer. |
| Couple's Mask | Completa: 1 de vida cada 4 frames de día, 1 de magia cada 7 de noche. |
| Mask of Truth | Slot vanilla y, al hablar a una Gossip Stone, hada normal y gran hada. Reutiliza el VB existente para las hadas aleatorias si están mezcladas. |
| Kamaro's Mask | Baile con B mantenido, como action func propia (también transformado: B es de la máscara). Tras 100 frames junto a Darunia cuenta como la Canción de Saria y él sigue su camino vanilla; no marca la recompensa por adelantado. |
| Gibdo Mask | Redeads amistosos bailando con las tres animaciones de MM; vuelven al estado de reposo al quitar la máscara. Si faltan animaciones, balanceo de respaldo. |
| Garo's Mask | Completa como traducción a OoT (guardias gerudo). `En_Ge2` consulta el VB cada frame, pero `En_Ge1` solo en su init: las gerudo de diálogo cambian al recargar la sala, las que patrullan al momento. |
| Captain's Hat | **Parcial**: Stalchild gigante / Stalfos cada 100 frames de noche en Hyrule Field (máx. 3) y esqueletos ciegos. Falta la reacción de los Deku Scrubs del escenario, que NEI conseguía mapeando el sombrero a `PLAYER_MASK_SKULL`. Ver petición 1. |
| Giant's Mask | **Parcial**: cutscene de máscara a la cara + relleno blanco + escala 0,025 + drenaje de magia con reversión a 0. Faltan la fuerza máxima al levantar y la resistencia a daño (petición 5) y los SFX de transformación de MM. |

## Recursos de mm.o2r

### Los que el extractor YA saca (y van en `requires`)

Rutas de 2ship, sin prefijo: `mm.o2r` se monta DEBAJO de los archivos de OoT, así que en una ruta que existe en
los dos juegos gana la de OoT. Display lists de máscara puesta:

```
objects/object_mask_posthat/object_mask_posthat_DL_000290
objects/object_mask_yofukasi/object_mask_yofukasi_DL_000490
objects/object_mask_bakuretu/object_mask_bakuretu_DL_0005C0
objects/object_mask_bakuretu/object_mask_bakuretu_DL_000440   (el de recarga)
objects/object_mask_stone/object_mask_stone_DL_000820
objects/object_mask_bigelf/object_mask_bigelf_DL_0016F0
objects/object_mask_bree/object_mask_bree_DL_0003C0
objects/object_mask_gero/gDonGeroMaskDL
objects/object_mask_bu_san/object_mask_bu_san_DL_000710
objects/object_mask_romerny/object_mask_romerny_DL_0007A0
objects/object_mask_zacho/object_mask_zacho_DL_000700
objects/object_mask_kerfay/gKafeisMaskDL
objects/object_mask_meoto/object_mask_meoto_DL_0005A0
objects/object_mask_dancer/object_mask_dancer_DL_000EF0
objects/object_mask_gibudo/object_mask_gibudo_DL_000250
objects/object_mask_json/object_mask_json_DL_0004C0
objects/object_mask_skj/object_mask_skj_DL_0009F0
objects/object_mask_kyojin/object_mask_kyojin_DL_000380
```

Modelos de get-item:

```
objects/object_gi_mask12/gGiPostmanHatCapDL
objects/object_gi_mask06/gGiAllNightMaskFaceDL
objects/object_gi_mask21/gGiBlastMaskDL
objects/object_gi_stonemask/gGiStoneMaskDL
objects/object_gi_mask14/gGiGreatFairyMaskFaceDL
objects/object_gi_mask20/gGiBremenMaskDL
objects/object_gi_rabit_mask/gGiBunnyHoodDL
objects/object_gi_mask16/gGiDonGeroMaskFaceDL
objects/object_gi_mask22/gGiMaskOfScentsFaceDL
objects/object_gi_mask10/gGiRomaniMaskCapDL
objects/object_gi_mask11/gGiCircusLeaderMaskFaceDL
objects/object_gi_mask05/gGiKafeiMaskDL
objects/object_gi_mask13/gGiCouplesMaskFullDL
objects/object_gi_truth_mask/gGiMaskOfTruthDL
objects/object_gi_mask17/gGiKamaroMaskDL
objects/object_gi_mask15/gGiGibdoMaskDL
objects/object_gi_mask09/gGiGarosMaskFaceDL
objects/object_gi_mask18/gGiCaptainsHatBodyDL
objects/object_gi_mask23/gGiGiantMaskDL
```

Animaciones del jugador:

```
misc/link_animetion/gPlayerAnim_clink_normal_okarina_walkB_Data   (marcha Bremen)
misc/link_animetion/gPlayerAnim_alink_dance_loop_Data             (baile Kamaro)
misc/link_animetion/gPlayerAnim_cl_msbowait_Data                  (olfateo Scents)
misc/link_animetion/gPlayerAnim_cl_setmask_Data                   (máscara a la cara)
misc/link_animetion/gPlayerAnim_cl_setmaskend_Data                (mantener la pose)
```

### Iconos y texturas de nombre

Salen de `archives/` e `interface/`, que el extractor de `mm_assets` ya saca (antes los excluía: ZAPD, compilado
con `GAME_OOT`, no descomprime los archivos *yar* de MM y reventaba; ahora `mm_assets` le pasa una copia de la
ROM con esos archivos ya descomprimidos). Las rutas son las de 2ship:

`icon_item_static_yar/` (17 iconos; la Bunny Hood y la Mask of Truth usan los de OoT y no hacen falta):

```
gItemIconPostmansHatTex      gItemIconAllNightMaskTex     gItemIconBlastMaskTex
gItemIconStoneMaskTex        gItemIconGreatFairyMaskTex   gItemIconBremenMaskTex
gItemIconDonGeroMaskTex      gItemIconMaskOfScentsTex     gItemIconRomaniMaskTex
gItemIconCircusLeaderMaskTex gItemIconKafeisMaskTex       gItemIconCouplesMaskTex
gItemIconKamaroMaskTex       gItemIconGibdoMaskTex        gItemIconGaroMaskTex
gItemIconCaptainsHatTex      gItemIconGiantsMaskTex
```

(y, para completar la rejilla desde los otros mods: `gItemIconDekuMaskTex`, `gItemIconKeatonMaskTex`,
`gItemIconGoronMaskTex`, `gItemIconZoraMaskTex`, `gItemIconFierceDeityMaskTex`.)

`item_name_static/` (los mismos 17, en ENG):

```
gItemNamePostmansHatENGTex        gItemNameAllNightMaskENGTex    gItemNameBlastMaskENGTex
gItemNameStoneMaskENGTex          gItemNameGreatFairysMaskENGTex gItemNameBremenMaskENGTex
gItemNameDonGerosMaskENGTex       gItemNameMaskOfScentsENGTex    gItemNameRomanisMaskENGTex
gItemNameCircusLeadersMaskENGTex  gItemNameKafeisMaskENGTex      gItemNameCouplesMaskENGTex
gItemNameKamarosMaskENGTex        gItemNameGibdoMaskENGTex       gItemNameGarosMaskENGTex
gItemNameCaptainsHatENGTex        gItemNameGiantsMaskENGTex
```

**No hay carpeta `assets/`**: todo lo de MM viene de `mm.o2r`, que extrae `mm_assets`.

## Peticiones al host

1. **Hook para resolver la máscara que lleva Link.** `OnPlayerResolveMask(Player* player, uint8_t* mask)` en
   `Player_GetMask` (`soh/src/code/z_player_lib.c:806`).
   - *Qué haría el mod*: la Mask of Truth y la Bunny Hood contestarían `PLAYER_MASK_TRUTH` / `PLAYER_MASK_BUNNY`
     sin escribir en `player->currentMask` ni tocar la CVar del usuario, y la Captain's Hat contestaría
     `PLAYER_MASK_SKULL`, que es lo único que le falta para que los Deku Scrubs del escenario la premien.
   - *Por qué no llega lo que hay*: `Player_ProcessItemButtons` (`z_player.c:2569`) borra `currentMask` cada
     frame en que el item de máscara vanilla no está en un botón, y un item custom nunca lo está. La única
     salida hoy es prestar `gEnhancements.PersistentMasks`, que es un ajuste del usuario, no del mod. Y
     `VB_DEKU_SCRUBS_REACT_TO_MASK_OF_TRUTH` **no sirve** para la Captain's Hat: vive dentro del `case
     PLAYER_MASK_TRUTH` de un `switch (Player_GetMask(play))`
     (`soh/src/overlays/actors/ovl_En_Dnt_Demo/z_en_dnt_demo.c:168`), así que un mod que no pueda cambiar la
     máscara que contesta `Player_GetMask` nunca llega a ese `case`.

2. **Hook para el modelo de máscara que dibuja el jugador.** `OnPlayerResolveMaskDList(Player*, Gfx** dList)`
   junto a `sMaskDlists[...]` (`soh/src/overlays/actors/ovl_player_actor/z_player.c:12526`).
   - *Qué haría*: prestar el slot vanilla (petición 1) **y** dibujar el modelo de MM en vez del de OoT.
   - *Por qué no llega*: `OnPlayerPostLimbDraw` corre después del limb de la cabeza, así que un mod puede
     añadir una máscara pero no sustituir la que vanilla ya puso; saldrían las dos.

3. **Darunia resuelto sin hook nuevo.** El mod pone `ocarinaMode = OCARINA_MODE_03` y a Darunia en
   `func_809FE4A4` (exportada), su espera de la Canción de Saria: su propio update lanza el cutscene y pasa a
   `func_809FE890` con la dirección real. Poner `func_809FE890` desde el mod fallaba dos veces: corría ese mismo
   frame, antes de que arrancara el cutscene, y saltaba directo a la entrega; y la dirección que ve una DLL es un
   thunk, así que las comparaciones de `actionFunc` de `z_en_du.c` no la reconocían.

4. **VB para las Gold Skulltulas nocturnas.** `VB_SPAWN_NIGHT_GOLD_SKULLTULA` en
   `soh/src/overlays/actors/ovl_En_Wood02/z_en_wood02.c:187` (el arbusto que las esconde) y en
   `soh/src/overlays/actors/ovl_En_Sw/z_en_sw.c:559`.
   - *Qué haría*: la All-Night Mask contestaría "es de noche" en el único sitio donde eso se decide.
   - *Por qué no llega*: `OnActorUpdate` filtrado por id ya reabre `En_Sw` (el mod lo hace), pero `En_Wood02`
     decide al **init** si esconde una skulltula, y no hay hook ahí con el params en la mano. Hoy el mod
     spawnea la tabla de posiciones a mano, que es lo mismo que hacía NEI.

5. **Hooks de fuerza y de daño recibido.** `OnPlayerResolveStrength(Player*, int32_t* strength)` en
   `Player_GetStrength` y `OnPlayerResolveIncomingDamage(Player*, int32_t* damage)` en el punto único donde
   `z_player.c` aplica `colChkInfo.damage`.
   - *Qué haría*: la Giant's Mask levantaría lo que levanta el guantelete de plata y recibiría un cuarto de
     daño, que es lo que hace en MM.
   - *Por qué no llega*: solo existe `VB_PREVENT_STRENGTH`, que quita fuerza pero no la concede, y
     `OnResolveSwordDamage` habla del daño que Link **hace**, no del que recibe.

6. **VB para la leche de la vaca.** `VB_COW_HEARD_EPONAS_SONG` alrededor del `DREG(53)` de
   `soh/src/overlays/actors/ovl_En_Cow/z_en_cow.c:257`.
   - *Qué haría*: la Romani's Mask diría "sí" ahí en vez de escribir el registro de depuración.
   - *Por qué no llega*: `DREG(53)` es global y compartido con la ocarina; escribirlo desde un mod puede
     colarse a otra vaca el mismo frame.

7. **Puente de SFX de Majora.** Una entrada de tabla `PlayMmSfx(uint16_t mmSfxId, Vec3f* pos)` o un kind de
   `OnActorPlaySfx` que acepte ids del Soundfont_0 de mm.o2r.
   - *Qué haría*: el gruñido de cerdo de la Mask of Scents y los cinco sonidos de la transformación de la
     Giant's Mask sonarían como en MM.
   - *Por qué no llega*: los ids de MM no existen en el banco de OoT y el mod no puede montar un soundfont
     ajeno por su cuenta.
