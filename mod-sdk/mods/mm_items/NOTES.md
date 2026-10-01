# mm_items — notas

Mod de Unbound con los dos items de Majora's Mask: **Powder Keg** (página 2, slot 0) y **Pictograph Box**
(página 2, slot 1). Todo vive en `mm_items.c` (970 líneas) + `assets/`.

## Alta en CMakeLists.txt (la hace el orquestador)

```cmake
unbound_add_mod(mm_items
    DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/mm_items"
    SOURCES mm_items/mm_items.c
)
target_link_libraries(mm_items PRIVATE z64items z64aiming)
```

`z64wheel` **no** hace falta: en NEI el keg compartía el slot de bombas y la pictobox el de la Lente, y la
rueda era la que decidía el modo. Aquí cada uno es un item de pleno derecho en su propio slot, así que no hay
nada que elegir. Además `z64wheel` es *una* rueda por mod (`sZ64Wheel` es un único estático), y este mod tiene
dos items: no encajaría aunque quisiéramos.

## Qué hace cada item

### Powder Keg

- `canUse`: hay kegs, `CUR_UPG_VALUE(UPG_STRENGTH) >= 2` (Guanteletes de Plata) y Link no carga nada.
- Item `INSTANT`: su `init` replica `Player_InitExplosiveIA` — `Actor_SpawnAsChild` de un `EN_BOM` de verdad
  con mecha de 100 ticks (el bombazo vanilla son 70), `heldActor`/`interactRangeActor`/`CARRYING_ACTOR`, y
  `func_80835688` para la pose y la animación de carga (vanilla llega a ella por el item action de la bomba;
  el keg tiene el suyo, así que la pide como el levantar una jarra). A partir de ahí carga, tira y suelta el
  keg el propio motor.
- `OnActorDraw` de `EN_BOM`: si es nuestro keg, dibuja el barril de MM (Opa) + calavera goron y mecha (Xlu) a
  escala 0.333 y apaga el dibujo vanilla. Sin `mm.o2r` se ve la bomba normal.
- Al explotar (`OnPlayerUpdate` vigila el `EN_BOM`): VFX `EffectSsBomb2_SpawnLayered` + onda de choque, 4
  `EN_BOM` satélite en anillo de 110 unidades con `timer = 1` (el radio máximo de una explosión son 72, así
  que se solapan con la del centro en vez de dejar un agujero) y barrido de obstáculos en 350 unidades:
  `BG_HEAVY_BLOCK` con `Actor_Kill` directo — **nunca** por su rotura propia, que son 12 actores de escombros
  cada uno y desbordaban el pool — y `EN_ISHI` / `OBJ_HAMISHI` / `OBJ_BOMBIWA` / `EN_GOROIWA`.
- Munición: `power_keg_count` en `ModStorage`, máximo 5. Llega lleno al recibir el item y **cada recarga de
  bombas (`ITEM_BOMBS_5..30`) suma un keg**. Sin eso un keg gastado se perdía para siempre: OoT no tiene
  ningún goron que los venda. Decisión abierta si prefieres otra fuente.

### Pictograph Box

- `init`: si hay foto guardada la enseña; si no, levanta el visor.
- Visor (`PICTO_LENS`): primera persona con `z64aiming`, A o el botón del item = disparo, B = guardar la caja.
  El disparo está desarmado hasta que sueltas los botones con los que entraste y hasta que la primera persona
  está de verdad activa (en NEI un guardia de "salta un frame" se colaba y disparaba al instante).
- Disparo: valida los sujetos **en ese instante** (posiciones buenas), congela el mundo con
  `RequestTimeControl(0.0f, freezeClock)` y encola la lectura del framebuffer.
- Lectura diferida: se emite en `OnPlayDrawEnd` — justo después del mundo y **antes** de `Interface_Draw`, así
  que la foto no lleva HUD — y se lee en el `OnPlayerUpdate` siguiente.
- Conversión verbatim de MM: RGBA16 → intensidad I8 → I5 comprimido (`PICTO_PHOTO_COMPRESSED_SIZE` = 11 200
  bytes, dentro del límite de 65 536 de `ModStorage`). La foto se enseña en sepia como MM
  (`G_CC_MODULATEI_PRIM` con prim 250/160/160), en franjas de 8 filas y con `gSPInvalidateTexCache` porque el
  búfer no se mueve y el caché de texturas seguiría sirviendo la foto anterior.
- Prompt "Keep this pictograph?" con `api->ShowTextbox(..., "\x1B%g&&Yes&No%w")`. Sí guarda en `ModStorage`,
  No la tira y te devuelve al visor (es como MM deja repetir la foto, y por eso nunca pregunta si reemplazar).
- Validación de sujetos portada de `mm/src/code/z_snap.c`: mismas comprobaciones (distancia, ángulo, región de
  captura, no tapado por bg ni por colisión de actor) y mismos flags, en `picto_flags` de `ModStorage`. MM
  enchufa la validación a un callback por actor que los actores de OoT no tienen, así que los sujetos son una
  tabla por `actor->id` (mono → En_Skj, Tingle → los kokiri, Lulu → Ruto, piratas → gerudos…).

## Trampas que ya se pagaron aquí

1. **El prompt no puede vivir en la upper action del item.** Con el textbox abierto la upper action deja de
   correr y el host lo lee como "el item ya no está en la mano" (`sHeldUpdateFrame`), así que se guarda solo.
   El prompt corre desde `OnPlayerUpdate` y llama a `Z64Items_KeepHeld` cada frame.
2. **`BlockPlayerInput` con `blockStick = true` mata la puntería.** El bloqueo filtra el `Input` que usa el
   jugador, y `func_8084ABD8` (la integración del stick que escribe `focus.rot`) lee ese mismo. Con el visor
   arriba solo se bloquean A y B; el stick se quita únicamente cuando ya está la foto en pantalla.
3. **Nada de `haltAllActors`.** Congela también a Link, y con él la upper action del item: el visor se quedaría
   muerto detrás de la foto. `TimeControl` con `worldSpeed = 0` para todo menos a Link, que es justo lo que
   hace falta.
4. **La posición del keg se copia cada frame.** Un actor muerto se libera antes de que `WatchKeg` pueda
   preguntarle dónde estaba.
5. **El visor deja el RDP en `G_CYC_FILL`** si no se restaura: el message box se dibuja después.

## Peticiones al host (numeradas)

Ninguna bloquea al mod; todas son cosas que hoy se resuelven peor de lo que se podrían resolver.

1. **Contador de munición para un item custom.**
   - Qué falta: un campo/callback en `SOHCustomItemDefinition` (por ejemplo `uint8_t (*getAmmo)(const char*)`)
     que el kaleido y el icono de botón dibujen como el número de bombas.
   - Qué haría el mod: enseñar los kegs que quedan (0–5) bajo el icono, como MM.
   - Por qué no llega lo que hay: `Z64Items_*` no expone munición y `OnInterfaceResolveButtonIcon` solo
     devuelve una ruta de textura; el mod tendría que redibujar el dígito encima del HUD por su cuenta.
   - Dónde iría: `soh/soh/ModApi/CustomItemRegistry/CustomItemRegistry.h:83` (struct) y el dibujo del contador
     en `soh/src/code/z_parameter.c` junto al de `AMMO(ITEM_BOMB)`.

2. **Sfx del obturador.** OoT no tiene el `NA_SE_SY_CAMERA_SHUTTER` de MM (0x4850), y el motor de audio de MM
   no está expuesto. Ahora suena `NA_SE_SY_CAMERA_ZOOM_DOWN`, que es lo más parecido del banco de OoT.
   - Qué haría el mod: sonar el clic de verdad al disparar.
   - Por qué no llega lo que hay: `RegisterAudioMix` sirve para mezclar PCM propio, pero eso significa meter
     el sample en el mod; si `mm_assets` acabara exponiendo el banco de MM sería gratis.
   - Dónde iría: tabla del ModApi, junto a `RegisterAudioMix`
     (`soh/soh/ModApi/ModApi.h:140`), como `PlayMmSfx(u16 sfxId, Vec3f* pos)`.

3. **Etiqueta del botón B mientras el visor está arriba.** MM pone "Stop" en B.
   `Interface_LoadActionLabelB` existe y se exporta, pero además hace falta poner `interfaceCtx.unk_1FA`, que
   no tiene función pública. Hoy el mod simplemente esconde el HUD con `Interface_ChangeAlpha(1)`.
   - Dónde iría: `soh/src/code/z_parameter.c`, un `Interface_SetActionLabelB(PlayState*, u16 action)` que haga
     las dos cosas.

4. **`gEnhancements.Items.ColorPictograph`.** La foto en color (RGBA16 en vez de sepia) existe en 2Ship y en
   NEI. Aquí se ha dejado fuera a propósito (el encargo pide la cadena RGBA16 → intensidad → I5), pero si se
   quiere, hace falta decidir dónde vive el CVar y su casilla de menú; el mod puede registrarla él con
   `RegisterMenuWidget`.

5. **Puente de la foto con MM (modo combo).** En NEI la foto se sincroniza con MM por un fichero lateral
   (`Picto_SyncWrite` / `Picto_SyncWriteColor`). Aquí la foto vive solo en `ModStorage` del mod
   (`picto_photo`, I5 de 11 200 bytes, más `picto_flags`), que es per-save. Para el combo haría falta o bien
   que `ModStorage` pueda escribir un blob fuera de la partida, o bien que ComboShip lea la entrada
   `mm_items/picto_photo` del save de soh. **Hueco declarado, no implementado.**

6. **Los flags de sujeto no los lee nadie en OoT.** `picto_flags` se escribe en el formato exacto de MM
   (`pictoFlags0/1`) para que un puente lo consuma. Sin ese puente, sacar la foto no da ninguna recompensa.

## Assets

Están en `assets/` y ya empaquetados en formato de recurso O2R (cabecera OTEX de 0x50 bytes, píxeles en 0x50;
verificado byte a byte contra `bomb_arrows/assets/**` antes de generarlos):

| Ruta | Formato | Estado |
|---|---|---|
| `textures/icon_item_custom/gItemIconPowerKegTex` | RGBA32 32×32 | **provisional**, dibujado a mano (barril + calavera + mecha) |
| `textures/icon_item_custom/gItemIconPictoBoxTex` | RGBA32 32×32 | **provisional**, dibujado a mano (caja + objetivo) |
| `textures/item_name_custom/gPowerKegNameTex` | IA4 128×16 | definitivo, generado con el `generate_names.py` de NEI (Century Gothic Bold) |
| `textures/item_name_custom/gPictoBoxNameTex` | IA4 128×16 | definitivo, mismo generador |

El `soh.o2r` de NEI **no trae** iconos de Powder Keg ni de Pictograph Box (los 106 de
`textures/icon_item_custom/` son de otros items): en NEI los dos reusaban el icono del slot que compartían.
Por eso los dos iconos son arte propio y deberían sustituirse por un render decente.

### De `mm.o2r` (opcionales, protegidos con `ResourceMgr_FileExists`)

| Ruta | Para qué |
|---|---|
| `objects/mm_object_gi_bigbomb/gGiPowderKegBarrelDL` | barril del keg en el suelo y en el get-item |
| `objects/mm_object_gi_bigbomb/gGiPowderKegGoronSkullAndFuseDL` | calavera + mecha (Xlu) |
| `objects/mm_object_gi_camera/gGiPictoBoxBodyAndLensDL` | modelo del get-item de la pictobox |
| `objects/mm_object_gi_camera/gGiPictoBoxFrameDL` | marco de la pictobox |

**No** van en `"requires"` del manifest a propósito: `requires` impide cargar el mod entero, y estos recursos
son cosméticos — sin `mm.o2r` el keg se ve como una bomba y la pictobox funciona igual. Es el caso que el
README describe para `HasResource`/`ResourceMgr_FileExists`.

### Lo que el extractor de MM todavía no saca

`sync_zapd_assets.py` deja fuera `interface`, así que `parameter_static` **no** está en el árbol de XML de
`mm_assets`. Faltan por eso:

- `parameter_static/gPictoBoxFocusBorderTex` (IA4 16×16, esquina del visor, espejada 4 veces)
- `parameter_static/gPictoBoxFocusIconTex` (I4 32×16, mirilla)
- `parameter_static/gPictoBoxFocusTextTex` (I4 32×8, cartel "PICTBOX")

Mientras no estén, el visor se dibuja por código en las posiciones exactas de los registros
`R_PICTO_FOCUS_*` de MM (esquinas en 80/60, 220/60, 80/160, 220/160; mirilla en 142/108). Si se añaden,
ojo: el prefijo de salida sería `mm_parameter_static`, y hay que pasarles la **ruta OTR** a
`gDPLoadTextureBlock_4b`, no el puntero resuelto, para que un pack HD de MM aplique.

## Fuera de alcance (a propósito)

- Foto en color (ver petición 4) y sincronización con MM (petición 5).
- Rueda del keg sobre el slot de bombas y de la pictobox sobre el de la Lente: aquí son items propios.
- Restricción por forma de MM (Fierce Deity / Goron / Gerudo): en OoT no hay formas, así que el requisito es
  solo Guanteletes de Plata.
- Modelo del keg en la mano de Link mientras lo carga: lo dibuja el `EN_BOM`, que es lo que Link lleva.
- Recompensas por foto: no existen en OoT (ver petición 6).
