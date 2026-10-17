# Cucui Ganon 0.10.0

Mod independiente para Shipwright-unbound con la nueva Mod API. Vive solo en `mod-sdk/mods/cucui-ganon` y se compila como `cucui-ganon.o2r`; no cambia el juego, el SDK ni `mod all`.

## Probarlo

El paquete se instala en `x64/soh-unbound-unsafe-windows/mods/cucui-ganon.o2r`. Activa **Enhancements → Cucui Ganon → Cucui Ganon battle** y entra a la batalla final. Desde Dev Tools, `warp 517 adult` lleva allí. Si el juego muestra **Unsigned mod**, elige **Load once**. Reinicia la escena para repetir. El usuario hace la prueba ingame; la compilación del paquete no confirma los efectos visuales ni la secuencia final.

La intro humana sustituye la transformación celeste. Conserva el brillo de la Trifuerza, gira a Ganondorf antes de bajarlo y corrige la suela de sus botas usando los vértices de `oot.o2r`. Los focos se encienden al final del baile de intro, antes de devolver cámara y control a Link. El fog de la arena permanece normal. El modelo usa `gGanondorfSkel` y los DL originales, permitiendo reemplazos de otros mods.

## Combate

Ganondorf comienza con **80 puntos internos** y llama a los cuatro Phantom cuando llega a **40**. En fase 1, los golpes y flechas por la espalda quitan 1 punto. Cada flecha por la espalda y cada esfera reflejada que le alcance acumulan un impacto; al tercero cae y queda vulnerable durante 120 actualizaciones. Una esfera que completa el contador quita 5 puntos. La electricidad acumulada se ve antes de caer, y el punto de fijación baja junto a él durante el aturdimiento.

El idle mira a Link y carga una bola visible en la mano, disparada en el giro siguiente. El giro con ambas manos arriba carga otra bola roja de fuego. Salsa 2 lanza tres bolas por cada mano, con trayectorias parabólicas **laterales en XZ** que descienden hasta el piso. Las bolas de la vuelta M5 también salen de las manos, suben en arcos y caen en ocho direcciones. El avance levanta rocas; el giro inmune manda una onda de explosiones y rocas hacia Link; el dash rápido tiene cono frontal y regresa al centro. Retroceso dispara una bola eléctrica más lenta que el resto. Las bolas tienen fuego, hielo, electricidad o explosión y pueden ser devueltas con la espada. El reflejo acepta un margen de espada de 55 unidades y revisa toda la trayectoria recorrida por la bola en cada actualización antes de aplicar daño. La colisión de daño con Link usa 20 unidades horizontales y solo la altura central de su cuerpo; el alcance de las bolas reflejadas contra Ganondorf permanece igual.

Los cuatro Phantom usan `object_gnd` a 80 % de tamaño, sin tridente visible. Sus animaciones de Blender se exportaron al esqueleto original: Salsa, paso lateral, giro, desplazamiento y dos miradas. Lanzan bolas desde las manos y sus vueltas o desplazamientos pueden golpear a Link. Se pueden fijar como objetivo. Cada uno muere por un golpe **desde su espalda**, por una **flecha bomba** del mod Bomb Arrows (la explosión puede alcanzarlo desde cualquier lado) o por una esfera reflejada dirigida a él. Mientras quede alguno, Ganondorf está protegido. Cuando mueren los cuatro, **una sola esfera reflejada** aturde al jefe; durante esa ventana vuelve a poder recibir golpes por la espalda. Cada recuperación cierra la ventana.

Al llegar Ganondorf a 1 punto, un golpe de espada por la espalda durante su aturdimiento activa el remate. Link usa la acción de corte final del juego; Ganondorf reproduce animaciones humanas de derrota del castillo, suelta sangre roja, grita mientras sube y se desintegra con partículas como un Phantom. El mod notifica la victoria con `ReportBossDefeated` (las estadísticas marcan la partida como completada, igual que con Ganon) y devuelve el control a Link. Aparece un portal azul en el centro. Al entrar, **corren los créditos**, como tras el Ganon vanilla. Esta coreografía y el portal requieren prueba ingame de cámara, efectos y transición.

Al encenderse los focos del intro aparece el cartel original de jefe, con **papi chulo** arriba y **Ganondorf** en grande. Las descripciones C-Up de Navi para Ganondorf y los Phantom están en inglés. Desde el comienzo del baile de combate se dibuja la letra española en una capa visual translúcida independiente. `ganondorf_lyrics.inc` conserva los 169 versos originales; el dibujo simplifica tildes porque la fuente de pantalla de OoT no incluye esos glifos. La capa nunca usa el sistema de mensajes del juego: Navi, el giro mágico y la pausa pueden funcionar mientras aparece la letra. Cuando aparece un mensaje real del juego, la letra deja de dibujarse y vuelve al terminar.

La primera sincronización divide la letra en 610 pulsos a **125 BPM** (4:52.8). Cada frase ocupa 2, 4 o 6 pulsos según su longitud. La letra vuelve al principio cuando termina y la música sigue sonando. Los tiempos por verso aún requieren comprobación ingame, porque la secuencia MIDI no incluye marcas de voz.

**Jefe final registrado:** el mod se registra con `RegisterBoss` en el hueco de Ganon (`SOH_BOSS_SLOT_GANON`) y pelea en su propia arena. Fuera del randomizer manda la casilla **Cucui Ganon battle**; en una semilla, la pelea entra si la casilla estaba activa al generarla, el spoiler lo apunta en `customBosses` y la partida lo conserva aunque luego se desactive la casilla. La lógica es la del Ganon vanilla (más estricta, nunca rompe la semilla). El Phantom sólo aparece invocado por este combate; no se anuncia como enemigo suelto al Enemy Randomizer. El mod no modifica el motor ni el SDK.

Para grabar todo M5 seguido con Link libre, activa también **M5 completo para grabar**. Las CVar son `gMods.CucuiGanon.DanceEnabled` y `gMods.CucuiGanon.PlayFullM5`.

## Música y fuentes

El mod interrumpe `NA_BGM_OPENING_GANON` cuando el juego lo inicia, reproduce intro RC1066 y entrada, después una pasada del loop sin las tres pistas `04 RC1066 ch3: Vocal Aahs Ganon`, y luego el loop completo. La frase dura 60 tiempos a 125 BPM; las notas de eco posteriores se trasladan al siguiente ciclo. Usa las fuentes 22, 27 y 32 de `oot.o2r`.

Las animaciones originales y todas las mallas se referencian por rutas OTR para mantener compatibilidad con los mods de recursos.
