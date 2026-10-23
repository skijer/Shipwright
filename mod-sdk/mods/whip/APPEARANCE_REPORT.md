# Apariencias remotas NEI — implementación ampliada

Fecha: 2026-10-17. Fuente: Shipwright-unbound-mod-nei/mod-sdk/mods. Este informe sustituye al anterior: los casos antes marcados «no expresable por appearance» se resolvieron con dibujo y estado de red dentro del propio mod.

## Resultado

32 mods incorporan un canal de estado visual independiente. Rod of Seasons y Sheikah Slate conservan appearance, y Somaria conserva su heldModelPath. No se editaron el host, los headers, el SDK compartido ni carpetas legacy. No se hizo commit ni stage.

Cada mod con dibujo de red declara el permiso network en su manifest. Si el juego solicita aprobar ese permiso al cargar un paquete actualizado, es necesario para que registre su canal visual. Ambos clientes necesitan los paquetes actualizados y sus recursos.

## Items y calibración

Los símbolos s* identifican los DL __OTR__ del propio mod. Las constantes y operaciones locales existentes se reutilizan, sin sustituirlas por una aproximación contra un único limb. Los campos appearance retirados en los casos ahora dibujados por el mod evitan superponer dos copias.

| Mod | Item | Uso | Anclaje | Modelo | Calibración existente | Estado actual |
|---|---|---|---|---|---|---|
| whip | `nei.whip` | en mano | PLAYER_LIMB_R_HAND | `sItemDL` | WHIP_HAND_ROT_X, WHIP_HAND_ROT_Y, WHIP_HAND_OFFSET_Y, WHIP_HAND_OFFSET_Z, WHIP_HELD_SCALE | hecho por network: Modelo enrollado o grip en mano derecha; cuerda/cabeza hacia punta o ancla durante la acción. |
| rod_of_seasons | `nei.rod_of_seasons` | en mano | PLAYER_LIMB_R_HAND | `sRodDL` | SEASONS_HELD_ROT_X, SEASONS_HELD_ROT_Y, SEASONS_HELD_OFFSET_Y, SEASONS_HELD_OFFSET_Z, SEASONS_HELD_SCALE | hecho mediante appearance; se conserva el camino declarativo simple |
| sheikah_slate | `nei.sheikah_slate` | en mano | PLAYER_LIMB_R_HAND | `sSlateDL` | SLATE_HELD_ROT_X, SLATE_HELD_ROT_Y, SLATE_HELD_OFFSET_Y, SLATE_HELD_SCALE | hecho mediante appearance; se conserva el camino declarativo simple |
| elemental_wand | `nei.elemental_wand` | en mano | PLAYER_LIMB_R_HAND | `sRods[WAND_SAND].opaModel` | WAND_HELD_ROT_X, WAND_HELD_ROT_Y, WAND_HELD_ROT_Z, WAND_HELD_OFFSET_Y, WAND_HELD_OFFSET_Z, WAND_HELD_SCALE | hecho por network: Modelo del modo seleccionado entre los seis elementos, con ambas pasadas y la matriz de mano derecha del remoto. |
| dominion_rod | `nei.dominion_rod` | en mano | PLAYER_LIMB_L_HAND | `sRodDL` | DOMROD_HELD_ROT_Z, DOMROD_HELD_OFFSET_Y, DOMROD_HELD_SCALE | hecho por network: Dibujo en mano izquierda, con las pasadas opaca y translúcida y reemplazo de la mano. |
| not_enough_bottles | `nei.net` | en mano | PLAYER_LIMB_L_HAND | `sNetDL` | NEB_NET_HELD_ROT_X/Y/Z, OFFSET_X/Y/Z, SCALE; conversiÃƒÂ³n XYZÃ¢â€ â€™YXZ por asin/atan2 | hecho por network: Cazamariposas completo, incluida la malla translúcida y la mano; no escribe puntos de captura durante dibujo remoto. |
| mogma_mitts | `nei.mogma_mitts` | puesto | PLAYER_LIMB_L_HAND | `sLeftOpenPlateDL` | DrawGauntlets: DL vanilla sin transformaciÃƒÂ³n, scale = 0 | hecho por network: Las cuatro placas originales, con selección abierta/puño de cada mano y visibilidad decidida por el emisor. |
| elemental_rods | `nei.rod_fire` | en mano | PLAYER_LIMB_L_HAND | `sFireRodDL` | DrawSwordReplacement: ROD_HELD_ROT_Z = -90, ROD_HELD_SCALE | hecho por network: Fire/Ice/Light en mano izquierda, con las dos pasadas cuando existen. |
| elemental_rods | `nei.rod_ice` | en mano | PLAYER_LIMB_L_HAND | `sIceRodDL` | DrawSwordReplacement: ROD_HELD_ROT_Z = -90, ROD_HELD_SCALE | hecho por network: Fire/Ice/Light en mano izquierda, con las dos pasadas cuando existen. |
| elemental_rods | `nei.rod_light` | en mano | PLAYER_LIMB_L_HAND | `sLightRodDL` | DrawSwordReplacement: ROD_HELD_ROT_Z = -90, ROD_HELD_SCALE | hecho por network: Fire/Ice/Light en mano izquierda, con las dos pasadas cuando existen. |
| mm_wearables | `nei.mask_postman` | puesto | PLAYER_LIMB_HEAD | `__OTR__objects/mask_posthat/object_mask_posthat_DL_000290` | DrawWornMask: DL en espacio de cabeza, scale = 0 | hecho por network: Todos los modelos worn propios, incluidos Great Fairy con pelo por jugador y Blast con su DL de enfriamiento. Las máscaras vanilla siguen currentMask. |
| mm_wearables | `nei.mask_all_night` | puesto | PLAYER_LIMB_HEAD | `__OTR__objects/mask_yofukasi/object_mask_yofukasi_DL_000490` | DrawWornMask: DL en espacio de cabeza, scale = 0 | hecho por network: Todos los modelos worn propios, incluidos Great Fairy con pelo por jugador y Blast con su DL de enfriamiento. Las máscaras vanilla siguen currentMask. |
| mm_wearables | `nei.mask_blast` | puesto | PLAYER_LIMB_HEAD | `__OTR__objects/mask_bakuretu/object_mask_bakuretu_DL_0005C0` | DrawWornMask: DL en espacio de cabeza, scale = 0 | hecho por network: Todos los modelos worn propios, incluidos Great Fairy con pelo por jugador y Blast con su DL de enfriamiento. Las máscaras vanilla siguen currentMask. |
| mm_wearables | `nei.mask_stone` | puesto | PLAYER_LIMB_HEAD | `__OTR__objects/mask_stone/object_mask_stone_DL_000820` | DrawWornMask: DL en espacio de cabeza, scale = 0 | hecho por network: Todos los modelos worn propios, incluidos Great Fairy con pelo por jugador y Blast con su DL de enfriamiento. Las máscaras vanilla siguen currentMask. |
| mm_wearables | `nei.mask_great_fairy` | puesto | PLAYER_LIMB_HEAD | `__OTR__objects/mask_bigelf/object_mask_bigelf_DL_0016F0` | DrawWornMask | hecho por network: Todos los modelos worn propios, incluidos Great Fairy con pelo por jugador y Blast con su DL de enfriamiento. Las máscaras vanilla siguen currentMask. |
| mm_wearables | `nei.mask_bremen` | puesto | PLAYER_LIMB_HEAD | `__OTR__objects/mask_bree/object_mask_bree_DL_0003C0` | DrawWornMask: DL en espacio de cabeza, scale = 0 | hecho por network: Todos los modelos worn propios, incluidos Great Fairy con pelo por jugador y Blast con su DL de enfriamiento. Las máscaras vanilla siguen currentMask. |
| mm_wearables | `nei.mask_bunny` | puesto | cabeza vanilla | `currentMask` | sin calibraciÃƒÂ³n custom | ya resuelto por currentMask; sin duplicar el modelo |
| mm_wearables | `nei.mask_don_gero` | puesto | PLAYER_LIMB_HEAD | `__OTR__objects/mask_gero/gDonGeroMaskDL` | DrawWornMask: DL en espacio de cabeza, scale = 0 | hecho por network: Todos los modelos worn propios, incluidos Great Fairy con pelo por jugador y Blast con su DL de enfriamiento. Las máscaras vanilla siguen currentMask. |
| mm_wearables | `nei.mask_scents` | puesto | PLAYER_LIMB_HEAD | `__OTR__objects/mask_bu_san/object_mask_bu_san_DL_000710` | DrawWornMask: DL en espacio de cabeza, scale = 0 | hecho por network: Todos los modelos worn propios, incluidos Great Fairy con pelo por jugador y Blast con su DL de enfriamiento. Las máscaras vanilla siguen currentMask. |
| mm_wearables | `nei.mask_romani` | puesto | PLAYER_LIMB_HEAD | `__OTR__objects/mask_romerny/object_mask_romerny_DL_0007A0` | DrawWornMask: DL en espacio de cabeza, scale = 0 | hecho por network: Todos los modelos worn propios, incluidos Great Fairy con pelo por jugador y Blast con su DL de enfriamiento. Las máscaras vanilla siguen currentMask. |
| mm_wearables | `nei.mask_circus_leader` | puesto | PLAYER_LIMB_HEAD | `__OTR__objects/mask_zacho/object_mask_zacho_DL_000700` | DrawWornMask: DL en espacio de cabeza, scale = 0 | hecho por network: Todos los modelos worn propios, incluidos Great Fairy con pelo por jugador y Blast con su DL de enfriamiento. Las máscaras vanilla siguen currentMask. |
| mm_wearables | `nei.mask_couple` | puesto | PLAYER_LIMB_HEAD | `__OTR__objects/mask_meoto/object_mask_meoto_DL_0005A0` | DrawWornMask: DL en espacio de cabeza, scale = 0 | hecho por network: Todos los modelos worn propios, incluidos Great Fairy con pelo por jugador y Blast con su DL de enfriamiento. Las máscaras vanilla siguen currentMask. |
| mm_wearables | `nei.mask_truth` | puesto | cabeza vanilla | `currentMask` | sin calibraciÃƒÂ³n custom | ya resuelto por currentMask; sin duplicar el modelo |
| mm_wearables | `nei.mask_kamaro` | puesto | PLAYER_LIMB_HEAD | `__OTR__objects/mask_dancer/object_mask_dancer_DL_000EF0` | DrawWornMask: DL en espacio de cabeza, scale = 0 | hecho por network: Todos los modelos worn propios, incluidos Great Fairy con pelo por jugador y Blast con su DL de enfriamiento. Las máscaras vanilla siguen currentMask. |
| mm_wearables | `nei.mask_gibdo` | puesto | PLAYER_LIMB_HEAD | `__OTR__objects/mask_gibudo/object_mask_gibudo_DL_000250` | DrawWornMask: DL en espacio de cabeza, scale = 0 | hecho por network: Todos los modelos worn propios, incluidos Great Fairy con pelo por jugador y Blast con su DL de enfriamiento. Las máscaras vanilla siguen currentMask. |
| mm_wearables | `nei.mask_captain` | puesto | PLAYER_LIMB_HEAD | `__OTR__objects/mask_skj/object_mask_skj_DL_0009F0` | DrawWornMask: DL en espacio de cabeza, scale = 0 | hecho por network: Todos los modelos worn propios, incluidos Great Fairy con pelo por jugador y Blast con su DL de enfriamiento. Las máscaras vanilla siguen currentMask. |
| mm_wearables | `nei.mask_giant` | puesto | PLAYER_LIMB_HEAD | `__OTR__objects/mask_kyojin/object_mask_kyojin_DL_000380` | DrawWornMask: DL en espacio de cabeza, scale = 0 | hecho por network: Todos los modelos worn propios, incluidos Great Fairy con pelo por jugador y Blast con su DL de enfriamiento. Las máscaras vanilla siguen currentMask. |
| mm_collectables | `nei.mm_remains_odolwa` | puesto | PLAYER_LIMB_HEAD | `__OTR__objects/object_ob/gMoonChildOdolwasMaskDL` | remains.c: sMaskScales * MASK_SCALE_CORRECTION, RotateZYX(180,-90,15), sMaskTranslateY/Z + MASK_OFFSET_Y/Z | hecho por network: Los cuatro remains conservan su calibración exacta de cabeza; también se dibujan espada y escudo de Odolwa. |
| mm_collectables | `nei.mm_remains_goht` | puesto | PLAYER_LIMB_HEAD | `__OTR__objects/object_ob/gMoonChildGohtsMaskDL` | remains.c: sMaskScales * MASK_SCALE_CORRECTION, RotateZYX(180,-90,15), sMaskTranslateY/Z + MASK_OFFSET_Y/Z | hecho por network: Los cuatro remains conservan su calibración exacta de cabeza; también se dibujan espada y escudo de Odolwa. |
| mm_collectables | `nei.mm_remains_gyorg` | puesto | PLAYER_LIMB_HEAD | `__OTR__objects/object_ob/gMoonChildGyorgsMaskDL` | remains.c: sMaskScales * MASK_SCALE_CORRECTION, RotateZYX(180,-90,15), sMaskTranslateY/Z + MASK_OFFSET_Y/Z | hecho por network: Los cuatro remains conservan su calibración exacta de cabeza; también se dibujan espada y escudo de Odolwa. |
| mm_collectables | `nei.mm_remains_twinmold` | puesto | PLAYER_LIMB_HEAD | `__OTR__objects/object_ob/gMoonChildTwinmoldsMaskDL` | remains.c: sMaskScales * MASK_SCALE_CORRECTION, RotateZYX(180,-90,15), sMaskTranslateY/Z + MASK_OFFSET_Y/Z | hecho por network: Los cuatro remains conservan su calibración exacta de cabeza; también se dibujan espada y escudo de Odolwa. |
| cane_pacci | `nei.cane_pacci` | en mano | mano derecha | `sCaneDL` | DrawPacciInRightHand: offset (1.64,-0.75,19.51), rot (83.14,62.96,1.08), escala 0.08 | hecho por network: Reutiliza DrawPacciInRightHand con antebrazo/mano del remoto y su calibración original. |
| shovel | `nei.shovel` | en mano | ambas manos | `sShovelDL` | SHOVEL_HELD_OFFSET_*, SHOVEL_HELD_ROTATION_*, SHOVEL_HELD_SCALE | hecho por network: Reconstruye punto medio y orientación desde ambas manos remotas. |
| beetle | `nei.beetle` | en mano | mano izquierda | `sBodyDL + sWingsDL` | BEETLE_HELD_REACH, BEETLE_HELD_SCALE | hecho por network: Modelo sostenido o volando, posición/orientación y apertura de alas del emisor. |
| ball_and_chain | `nei.ball_and_chain` | en mano | ambas manos | `sBallDL + sChainLinkDL` | BALL_HELD_SCALE; DrawBall y DrawChain | hecho por network: Bola, cadena entre ambas manos y cadena hacia la bola; arco dorado según estado del emisor. |
| deku_leaf | `nei.deku_leaf` | en mano | mano izquierda / ambas manos | `sLeafDL` | DEKU_LEAF_HELD_SCALE, DEKU_LEAF_CANOPY_* | hecho por network: Modelo en mano o canopy según estado del emisor; también se transmite el estado de barrido para la escala. |
| gust_jar | `nei.gust_jar` | en mano | ambas manos | `sJarBodyDL + sJarDecorationDL` | GUST_JAR_HAND_DROP, GUST_JAR_HELD_SCALE | hecho por network: Ambas manos remotas, cuerpo y decoración, color del elemento del emisor. |
| lantern | `nei.lantern` | en mano | mano / cintura | `sLanternDL` | LANTERN_HELD_SCALE; DrawLantern | hecho por network: Posición de mano remota, modelo opaco/translúcido y llama según elemento del emisor. |
| phantom_hourglass | `nei.phantom_hourglass` | en mano | mano | `gPhantomHourglassDL` | HOURGLASS_HAND_DROP, HOURGLASS_HELD_SCALE | hecho por network: Modelo en la mano izquierda remota; no dibuja la retícula ni la ruta del cliente local. |
| switch_hook | `nei.switch_hook` | en mano | mano derecha | `sGiveDL` | SWITCH_HOOK_HELD_SCALE | hecho por network: Modelo sostenido y cadena/punta en vuelo o retorno. |
| spinner | `nei.spinner` | puesto (montado) | raÃƒÂ­z / pies | `sSpinnerDL` | SPINNER_MODEL_SCALE | hecho por network: Modelo bajo el jugador remoto con giro animado. |
| somaria | `nei.somaria` | en mano | PLAYER_LIMB_L_HAND | `sCaneDL` | Z64Items_SetHeldModel existente | hecho previamente mediante fallback heldModelPath; sin cambio |

En máscaras, los alias legacy conservan sus claves; el estado visual identifica el índice del catálogo de esta versión del mod. Los avisos SetCustomItemWorn existentes se mantienen para el estado estándar de Anchor.

## Equipamiento y otros dibujos cubiertos

SOHCustomEquipDefinition sigue sin appearance. Estos mods usan su propio canal y sus hooks de dibujo, sin crear otro gemelo de inventario ni editar el host.

| Mod | Implementación |
|---|---|
| byrna | Bastón en mano izquierda y reemplazo de la mano con las transformaciones y escalas originales. |
| four_sword | Espada de cuatro colores y mano apropiada a la edad; cada dibujo tiene su propio DL para no mezclar jugadores. |
| trident | Modelo de lanza en mano izquierda con su calibración original y segmento propio enlazado en el mod. |
| divine_shield | Modelo en mano o espalda, ocultando la pieza vanilla correspondiente del remoto. |
| kite_shield | Modelo en mano/espalda o tabla bajo el jugador, con giro de tabla del emisor. |
| ikana_shield | Modelo en mano y espalda; el emisor excluye el caso en que Odolwa se queda el escudo. |
| magic_cape | Tela simulada por jugador desde sus dos hombros; vértices propios por dibujo y segmento 0x0C restaurado. |
| pegasus_boots | Ambas botas rojas para adulto y cono de viento durante carrera. |
| climb_boots | Modelos en ambos pies usando las matrices remotas, con ajuste infantil y tinte originales. |
| roc_boots | Botas doradas en ambos pies con ajuste infantil y tinte originales. |
| champion_tunic | Color producido por DyeTunic del emisor. |
| magic_tunic | Color de DyeTunic según dinero/estado del emisor. |
| sages_tunic | Color de DyeTunic, incluyendo mezcla/flash de resistencia del emisor. |
| pendant | Cue de parry en la mano, con estado armado del emisor. |

## Estado por jugador y recursos

- Se usa RegisterNetChannel + SetNetPeerState/GetNetPeerState. El host conserva el último estado para nuevas conexiones; no son eventos que un jugador que entra tarde perdería.
- Cada paquete tiene 60 bytes, escena, visibilidad, edad y campos visuales de tamaño fijo; nunca contiene punteros o ids locales de items/actores.
- Se publica solo si cambió el estado, con un intervalo mínimo de cinco frames de simulación: hasta cuatro actualizaciones por segundo a 20 Hz. No se mandan paquetes en cada frame. El límite global de la API sigue aplicándose; si varias acciones cambian a la vez, el host puede retrasar/coalescer estados.
- Se comprueba tamaño exacto, escena, actor asociado a peer, estado activo, índice de catálogo y finitud/rango de números antes de dibujar. Al desconectar, el host retira sus estados. No se guardan punteros de actores entre frames.
- Anchor cambia el id del dummy a ACTOR_EN_OE2. Los modelos dibujados al final del actor tienen un dispatcher global que consulta GetNetPeerIdForActor, en lugar de confiar únicamente en un hook filtrado por ACTOR_PLAYER.
- Los hooks de limb se habilitan con el permiso network. Las capturas y efectos que solo pertenecen al local tienen guardas para no utilizar su estado sobre otro jugador.
- Las pasadas de mano restauran la matriz y el color/material del jugador. Los dibujos remotos del cazamariposas no alteran los puntos usados por la captura local.
- Great Fairy tiene cadenas de pelo y frame de física por peer; se enlaza su segmento de matrices al dibujar ese jugador. Se preservan y restauran los arrays y parámetros del cliente local.
- Magic Cape tiene cadenas y estado de tela por peer y escena, velocidad del emisor y capturas de ambos hombros. Los vértices remotos se asignan en Graph_Alloc para cada dibujo: no se reescribe un buffer compartido que otras capas ya estén usando. Después se restaura el segmento 0x0C y el estado local.
- Los caches de pelo y capa admiten 64 peers, recuperan entradas de jugadores desconectados y reinician al cambiar de escena o tras un intervalo sin dibujar.

## Límites de esta entrega

La representación visual de los modelos enumerados está implementada. No se ha convertido cada mecánica de combate, actor invocado o efecto del mundo en una simulación multijugador:

- La posición de objetos animados como bola, punta del látigo o beetle es una muestra visual con frecuencia limitada; no es una colisión remota ni una simulación autoritativa. Puede verse escalonada o retrasada según el tráfico.
- No se replican los clones de Four Sword, proyectiles/áreas de daño de Trident y varas, aliados de los remains, construcción de Ultrahand, portales de Time Gate ni la ruta de rebobinado de Phantom Hourglass.
- Los trails de arma y efectos de combate auxiliares de Byrna/Trident siguen locales. Los tintes de túnica, pelo/capa, enfriamiento de Blast, llamas, cadenas, alas y cono Pegasus sí están en el alcance visual implementado.
- La talla gigante de Giant's Mask y el control de formas TAKEOVER no se convierten en estado de red por esta entrega; son distintos del DL de la máscara.
- Formas de transformación quedan excluidas como en la tarea original. La espada de Fierce Deity no es un item custom independiente, sino geometría de la forma.
- minish_cap, mm_items (Picto/Keg), Chateau, botellas extra/bottomless, rocs, spiritual_stones, easy_equipment, bomb_arrows, demise_destruction, zonai_permafrost y useful_medallions no aportan otro DL propio calibrado de prenda/objeto de reposo sobre Link que trasladar por este mecanismo. Se conservan sus comportamientos y actores originales.

Estos límites no se presentan como imposibilidades de la API: ampliar actores, impactos, rutas y efectos requeriría otra implementación de estado visual o de gameplay.

## Verificación

- Compilación individual de los grupos cambiados y compilación/instalación completa con .\build-all.local.ps1 -Only Mods -Reconfigure desde Shipwright-unbound.
- Harness nativo con los cuerpos reales ReadRemoteVisual de los 32 mods, API simulada y el emisor real de Shovel: pruebas de tamaño corto/largo/ausente, escena incorrecta, actor local, peer inexistente, estado inactivo, edad/índice inválidos, NaN, infinito, magnitud excesiva y dibujo deshabilitado. Pasan.
- El emisor pasa pruebas de límite de frecuencia, no duplicar estado idéntico, retirada, cambio de escena, descarga de partida y reintento tras fallo al publicar.
- Revisión de buffers por jugador para pelo, capa y DL de Four Sword, conservación del estado local y restauración del material de mano.
- git diff --check sin errores.
- La comprobación visual con dos clientes Anchor sigue pendiente. Las pruebas aisladas y la compilación no equivalen a esa prueba.

## Compilación e instalación por mod tocado

| Mod | Compilación/paquete | Instalación |
|---|---|---|
| ball_and_chain | Correcta; DLL del .o2r coincide con el build | Instalado; SHA-256 coincide |
| beetle | Correcta; DLL del .o2r coincide con el build | Instalado; SHA-256 coincide |
| byrna | Correcta; DLL del .o2r coincide con el build | Instalado; SHA-256 coincide |
| cane_pacci | Correcta; DLL del .o2r coincide con el build | Instalado; SHA-256 coincide |
| champion_tunic | Correcta; DLL del .o2r coincide con el build | Instalado; SHA-256 coincide |
| climb_boots | Correcta; DLL del .o2r coincide con el build | Instalado; SHA-256 coincide |
| deku_leaf | Correcta; DLL del .o2r coincide con el build | Instalado; SHA-256 coincide |
| divine_shield | Correcta; DLL del .o2r coincide con el build | Instalado; SHA-256 coincide |
| dominion_rod | Correcta; DLL del .o2r coincide con el build | Instalado; SHA-256 coincide |
| elemental_rods | Correcta; DLL del .o2r coincide con el build | Instalado; SHA-256 coincide |
| elemental_wand | Correcta; DLL del .o2r coincide con el build | Instalado; SHA-256 coincide |
| four_sword | Correcta; DLL del .o2r coincide con el build | Instalado; SHA-256 coincide |
| gust_jar | Correcta; DLL del .o2r coincide con el build | Instalado; SHA-256 coincide |
| ikana_shield | Correcta; DLL del .o2r coincide con el build | Instalado; SHA-256 coincide |
| kite_shield | Correcta; DLL del .o2r coincide con el build | Instalado; SHA-256 coincide |
| lantern | Correcta; DLL del .o2r coincide con el build | Instalado; SHA-256 coincide |
| magic_cape | Correcta; DLL del .o2r coincide con el build | Instalado; SHA-256 coincide |
| magic_tunic | Correcta; DLL del .o2r coincide con el build | Instalado; SHA-256 coincide |
| mm_collectables | Correcta; DLL del .o2r coincide con el build | Instalado; SHA-256 coincide |
| mm_wearables | Correcta; DLL del .o2r coincide con el build | Instalado; SHA-256 coincide |
| mogma_mitts | Correcta; DLL del .o2r coincide con el build | Instalado; SHA-256 coincide |
| not_enough_bottles | Correcta; DLL del .o2r coincide con el build | Instalado; SHA-256 coincide |
| pegasus_boots | Correcta; DLL del .o2r coincide con el build | Instalado; SHA-256 coincide |
| pendant | Correcta; DLL del .o2r coincide con el build | Instalado; SHA-256 coincide |
| phantom_hourglass | Correcta; DLL del .o2r coincide con el build | Instalado; SHA-256 coincide |
| roc_boots | Correcta; DLL del .o2r coincide con el build | Instalado; SHA-256 coincide |
| rod_of_seasons | Correcta; DLL del .o2r coincide con el build | Instalado; SHA-256 coincide |
| sages_tunic | Correcta; DLL del .o2r coincide con el build | Instalado; SHA-256 coincide |
| sheikah_slate | Correcta; DLL del .o2r coincide con el build | Instalado; SHA-256 coincide |
| shovel | Correcta; DLL del .o2r coincide con el build | Instalado; SHA-256 coincide |
| spinner | Correcta; DLL del .o2r coincide con el build | Instalado; SHA-256 coincide |
| switch_hook | Correcta; DLL del .o2r coincide con el build | Instalado; SHA-256 coincide |
| trident | Correcta; DLL del .o2r coincide con el build | Instalado; SHA-256 coincide |
| whip | Correcta; DLL del .o2r coincide con el build | Instalado; SHA-256 coincide |

Build final: código 0. Los 66 paquetes del build coinciden por SHA-256 con los instalados. Se verificó la DLL incluida en los 34 mods tocados y el permiso network en los manifests empaquetados.
