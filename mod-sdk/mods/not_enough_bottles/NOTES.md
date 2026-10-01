# NotEnoughBottles — notas

## Qué hace

- **Rejilla vanilla**: `[1 3 5 7] [2 4 6 8] [Net] [Bottomless] [trade adulto] [trade niño]`. Las botellas 1-4
  son las vanilla; el mod solo mueve la 3 y la 4 a las dos primeras celdas con `PlaceLayoutItem`
  (`vanilla.0.20` y `vanilla.0.21`). Las 5-8 son items custom (`nei.bottle_5..8`) colocados en esas mismas
  celdas con prioridad negativa, así la rueda sale en orden.
- **Contenido**: cada botella custom (y la Bottomless) guarda su contenido en `ModStorage`
  (`not_enough_bottles/custom_bottles`). El icono es el del contenido (`CustomItemRegistry_SetIconPath` con
  `gItemIcons`) y el nombre de pausa sale por `OnKaleidoResolveName` en los tres idiomas.
- **Sin hooks nuevos** (decisión del usuario):
  - `OnPlayerResolveItemAction`: un botón con una de estas botellas resuelve la acción de su contenido
    (vía `heldItemButton`), así beber, soltar, pescar, vender y enseñar son el código vanilla.
  - `VB_UPDATE_BOTTLE_ITEM`: la escritura vuelve al mod en vez del inventario. Vanilla pisa después el botón
    con un id vanilla (`z_parameter.c:2736`); el mod le devuelve `ITEM_CUSTOM` en `OnPlayerUpdate` y en el
    `OnActorUpdate` de `En_Gb`/`En_Hy`, más `OnGameFrameUpdate` de red. La lectura fuera de rango de
    `items[0xFF]` en esa función se queda (el usuario prefirió no tocar el motor); por eso la media leche se
    recalcula contra el contenido real.
  - `OnResolveItemGive`: vanilla llena primero sus cuatro; lo que no cabe va a las del mod (una botella nueva
    se convierte en la siguiente custom no poseída).
  - **Préstamo de slot** (`ShouldActorUpdate` → `OnActorUpdate`): los NPC que solo miran los 4 slots vanilla
    (`Inventory_HasEmptyBottle` en tiendas, vaca, Talon, Dns, Granny, Poes; `HasSpecificBottle` del mendigo;
    `Inventory_ConsumeFairy` con Link a 0 corazones) ven durante su update el contenido de una botella del
    mod escrito en un slot vanilla, y al cerrar la ventana se devuelve y el mod se queda con lo que cambió.
  - `OnFlagSet(EVENTCHKINF_KING_ZORA_MOVED)`: la carta de Ruto en una botella custom pasa a vacía.
- **Bottomless**: botella custom más con contador de usos (`getAmmo` en la pausa, dígitos en el botón).
- **Net**: item propio; la captura entra por `Item_Give`, así va al mismo sitio que una captura vanilla.
- **Migración**: una partida del diseño anterior (8 botellas proyectadas en los slots 1/2/4) reparte las
  ocultas en slots vanilla libres y luego en las botellas nuevas.

## Huecos conocidos

1. En las 6 escenas con restricción de botellas (Chamber of Sages, galería, patio del castillo, pesca,
   bolos) las botellas custom no se atenúan: la restricción mira el rango de ids de botella.
2. `Item_CheckObtainability` elige fanfarria mirando solo los slots vanilla: cosmético.
3. Las 5-8 aún no están en el pool del randomizer: falta la entrega en runtime de `RandoItems`. Hoy se
   consiguen cuando el juego da una botella con las 4 vanilla ya ocupadas, o con `give custom_item`.
