# Anim Viewer

Abre **Dev Tools → Anim Viewer** y pulsa **Refresh**.

- Escribe en el buscador para filtrar por nombre o ruta. Los presets permiten buscar vault, spin y cortes.
- Selecciona el archivo de origen en **Source** y una animación en el dropdown o en la lista con scroll.
- **Previous / Next** cambia de clip. **Play on Link**, **Pause / Resume**, **Stop** y **Restart clip** controlan la reproducción.
- **Frame** permite arrastrar hasta una pose; los botones de frame avanzan de uno en uno. **Speed** incluye cámara lenta y reversa. **Loop** repite el clip.
- **Lock root translation** bloquea el desplazamiento visual de la raíz. El actor permanece en su posición durante la vista previa.
- Los botones **Copy animation path / Copy raw clip path** copian las rutas al portapapeles.
- **Mark for Byrna** guarda la animación y el frame. Pulsa un candidato para copiarlo o usa **Export candidates** para guardarlos en `mods/animviewer_byrna.txt`.

Inicia la vista previa con Link en suelo seguro y fuera de diálogos o cinemáticas. **Stop** devuelve el control al juego. No se modifica el inventario; Link guarda el objeto que llevaba en la mano al iniciar.

## Otros O2R

**Import O2R** abre el selector del juego y guarda el archivo elegido en `mods`. Después reinicia y pulsa **Refresh**. Usa bibliotecas de recursos de animación; un paquete que contiene otros mods también cargará esos mods al reiniciar. **Include resources with custom names** busca también clips sin «anim» en su ruta.

Compatible con animaciones del esqueleto humano de Link de 67 valores por frame, incluidos clips personalizados. Los clips de NPC u otras formas requieren su propio esqueleto y no se reinterpretan como Link.

## Requisito de versión

Esta versión usa los nuevos controles generales `ModUI` del host actualizado. Las funciones están disponibles para todos los mods; no dependen de Randomizer. Importar una biblioteca sigue requiriendo reiniciar porque el montaje de archivos en caliente no forma parte de esta ampliación de UI.

La reproducción en una partida queda pendiente de prueba del usuario.
