# Videos y documentos del proyecto

Aquí está el código con el que se hicieron los dos videos (técnico y comercial) y los dos dossiers en PDF. Todo es gratis: Three.js para el 3D, Chromium (Playwright) para dibujar cada cuadro, FFmpeg para montar y numpy para el sonido.

Los archivos terminados **no están en el repositorio**: los videos pesan unos 70 MB cada uno, y los videos y los PDF muestran mi cara, mi casa y mi cuarto. Quedan en local:

| Archivo | Dónde queda |
| --- | --- |
| `Centinela_RF_comercial_v2.mp4` (58 s, vertical) | `C:\Users\juanc\OneDrive\Documentos\io\Semestre02_ES_IA\IoT\` |
| `Centinela_RF_tecnico_v2.mp4` (2:45, horizontal) | misma carpeta |
| `Centinela_RF_comercial_borrador.mp4` y `Centinela_RF_tecnico_borrador.mp4` (primera versión) | misma carpeta |
| `Centinela_RF_Dossier_Tecnico.pdf` (22 páginas) | misma carpeta |
| `Centinela_RF_Dossier_Inversionistas.pdf` (11 páginas) | misma carpeta |
| Material grabado (DJI, celular, pantalla) | misma carpeta |

## Carpetas

| Carpeta | Qué tiene |
| --- | --- |
| `escenas/` | Páginas que dibujan cada escena animada. `dron.html`: enjambre de drones con visor (`?o=v` vertical u `?o=h` horizontal; `?t=com` o `?t=tec`). `escena.html` y `escena_h.html`: las tres placas en 3D que se arman y giran. `tec.html`: diagramas del video técnico (`?e=arq`, `rf`, `res`, `atajo`, `cnn`, `cnn2`, `lim` y las capas `o_titulo`, `o_app`, `o_dev`). `espectro.js`: filas reales del nRF24 para la cascada. `capas_vertical.html` y `avisos_hud.html`: fondos, marcos, visor y el aviso "SEÑAL DE DRON DETECTADA" |
| `scripts/` | `grabar.js` y `grabar2.js` dibujan los cuadros con Playwright. `sonido.py` tiene los sonidos sintéticos (hélices, golpes, latido, alarma, música) y `audio_comercial.py` / `audio_tecnico.py` arman cada pista |
| `subtitulos/` | Subtítulos `.ass` de cada video |
| `dossier/` | HTML y estilos de los dos PDF, gráficas en SVG y `hacer_pdf.js`. Las fotos del montaje, la ventana y la cámara de gestos no se suben |

## Cómo se arman

1. `npm install three@0.169.0` dentro de `escenas/`, y servir esa carpeta: `python -m http.server 8765`.
2. Dibujar cada escena cuadro a cuadro, por ejemplo el enjambre vertical de 7 s a 30 fps:
   `node scripts/grabar2.js "http://localhost:8765/dron.html?o=v&t=com" D1 0 7 0.0333333333 1080 1920`
3. Cortar el material con FFmpeg (los segundos de cada toma están en la tabla de abajo) y codificar cada escena por separado, con el mismo tamaño, 30 fps y `libx264 -crf 18`.
4. Tratamiento de cine en las tomas reales: `colorbalance` (sombras frías, luces cálidas), grano (`noise`), viñeta y `rgbashift`. La toma térmica es `format=gray,format=yuv444p,pseudocolor=p=inferno`, con el visor `avisos_hud.html` encima y la altura y distancia con `drawtext`.
5. Unir las escenas con el *concat demuxer* y `-c copy`.
6. `python scripts/audio_comercial.py audio.wav` (o `audio_tecnico.py`).
7. Una sola pasada final: subtítulos (`subtitles=…ass`) y sonido, AAC a 192 kbps.

## Escenas del comercial v2 (vertical 1080x1920, 58 s)

| Tiempo | Escena | Material |
| --- | --- | --- |
| 0:00 | Enjambre de drones con visor; contador de ataques 61 → 333 | `dron.html?o=v&t=com` |
| 0:07 | Toma del dron en cámara térmica con mira | DJI 0017, segundos 44 a 48 |
| 0:11 | El dron baja hacia la ventana | DJI 0017, segundos 50 a 56 |
| 0:17 | "SEÑAL DE DRON DETECTADA" con temblor | `avisos_hud.html` |
| 0:18 | Las placas se arman en 3D | `escena.html?e=ensamble` |
| 0:24 | La app pasa a alerta, dentro de un celular | Grabación de pantalla 16-08-03, segundos 15,5 a 23,5 |
| 0:32 | El montaje real | IMG_7495, segundos 3 a 9 |
| 0:38 | La cámara reconoce la mano abierta | Comet 16-11-31, segundos 34 a 42 |
| 0:46 | Las placas giran con las cifras | `escena.html?e=giro` |
| 0:52 | Panorámica de la ciudad y cierre | DJI 0019, segundos 46 a 52 |

## Escenas del técnico v2 (horizontal 1920x1080, 2:45)

| Tiempo | Escena |
| --- | --- |
| 0:00 | Enjambre con cifras de UNMAS y la Defensoría |
| 0:08 | Título sobre la toma térmica del dron (DJI 0017, 0 a 8 s) |
| 0:16 | El dron entra al cuarto (DJI 0017, 72 a 80 s) |
| 0:24 | "SEÑAL DE DRON DETECTADA" |
| 0:25 | Placas en 3D con sus nombres |
| 0:35 | Arquitectura con pines y protocolos |
| 0:49 | Espectro real y los 6 pasos del pipeline |
| 1:03 | Matriz de confusión y métricas |
| 1:15 | El atajo del tráfico WiFi |
| 1:24 | La app en alerta |
| 1:36 | El devkit grabando y reentrenando |
| 1:48 | La CNN capa por capa |
| 2:01 | Entrenamiento, recall y regla de disparo |
| 2:13 | Demo del gesto (Comet 16-11-31, 30 a 44 s) |
| 2:27 | Límites y siguientes pasos |
| 2:37 | Cierre en 3D |

## Fuentes de las cifras de contexto

- UNMAS, citado por El País (17 de abril de 2026): 61 ataques con drones en 2024 y 333 en 2025.
- Defensoría del Pueblo, citado por Al Jazeera / AFP (18 de septiembre de 2026): +146 % entre enero y mayo de 2026.
