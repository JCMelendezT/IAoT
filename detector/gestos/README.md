# Gestos para la cámara

Entrena la red que reconoce **mano abierta**, **puño** y **nada** en la [ESP32-CAM](../../firmware/camara_gestos/). Solo necesita Python 3 y NumPy (`pip install numpy`).

```
fotos/     los .zip que baja la página de la cámara (no se suben a GitHub)
modelo/    sale modelo_gestos.bin (el que se sube a la cámara) y ficha_gestos.md
```

## Uso

```
python entrenar_gestos_torch.py              # RECOMENDADO (necesita: pip install torch). Red de 4 capas, usa la GPU si la ve
python entrenar_gestos_torch.py --epocas 120 # si la exactitud se queda corta
python entrenar_gestos.py                    # version sin torch (solo NumPy), red de 3 capas, mas floja
python entrenar_gestos.py --autoprueba # comprueba el propio código con fotos inventadas (no usa tus fotos)
python verificar_firmware.py           # comprueba que el C++ de la cámara da lo mismo que NumPy (necesita g++)
```

Las fotos llevan el nombre `<clase>_s<sesión>_<número>.pgm` dentro de cada `.zip` (por ejemplo `puno_s03_0012.pgm`), como las deja la página de la cámara. También se leen `.pgm` sueltos.

## Sin sacar fotos: dataset público (HaGRID)

[HaGRID](https://github.com/ai-forever/hagrid) (Kapitanov et al., WACV 2024, licencia CC BY-SA 4.0) tiene fotos de miles de personas con `palm` (mano abierta), `fist` (puño) y `no_gesture`. La versión reducida [`hagrid-sample-30k-384p`](https://huggingface.co/datasets/cj-mills/hagrid-sample-30k-384p) pesa 1 GB.

1. Baja `hagrid-sample-30k-384p.zip` y descomprímelo en `detector/gestos/publico/`.
2. `pip install numpy pillow`
3. `python preparar_publico.py publico` recorta alrededor de la mano, la pasa a 80x60 en gris y escribe `fotos/publico_abierta.zip`, `publico_puno.zip` y `publico_nada.zip`, con cada persona como una sesión.
4. `python entrenar_gestos_torch.py` entrena con esos zips (y con los de la cámara, si los hay).

Limitación: son fotos de otras cámaras y cuartos. En la cámara real la exactitud suele ser menor que la que muestra el entrenador; con unas pocas tandas propias (sobre todo de "nada" con tu cuarto) mejora bastante. Las fotos propias llevan sesiones `s01`, `s02`...; las públicas, `s100` en adelante.

## Qué mirar en el resultado

- Cada **sesión** (una tanda de fotos) se queda completa en entrenamiento o en validación, nunca partida. Si se mezclaran fotos de la misma tanda, parecerían casi iguales y la exactitud saldría inflada. Por eso piden al menos 3 sesiones por gesto.
- La **matriz de confusión** dice qué gesto se confunde con cuál.
- Los **disparos falsos por hora** aplican la misma regla que la cámara (5 de 6 fotos con 80 % de seguridad, y volver a "nada" antes de otro gesto) sobre las sesiones de "nada". Es lo que más importa: una orden que se dispara sola molesta más que una que a veces hay que repetir. Se calcula suponiendo 4 fotos por segundo; la cámara real puede ir más lenta (la página muestra los ms de cada inferencia).
- Si "nada" se confunde con algún gesto, faltan fotos de "nada" con el fondo y las poses reales de tu cuarto.

## La red

Entrada 80x60 en gris, tres capas de convolución 3x3 (8, 16 y 24 filtros) con ReLU y submuestreo, promedio global y una capa final de 3 salidas. 4803 números (19 KB). Se entrena con Adam, con giro, desplazamiento, brillo, contraste y ruido aleatorios. Está hecha a mano con NumPy a propósito: la cámara ejecuta el mismo cálculo sin librerías de IA y el modelo viaja como un archivo que se cambia por WiFi, sin recompilar. Se descartó Edge Impulse porque obliga a exportar una librería y recompilar la cámara cada vez que cambia el modelo.

El formato de `modelo_gestos.bin` (cabecera, los números en `float32` y una suma de comprobación) está en `entrenar_gestos_torch.py` y en `cnn_gestos.h`. La versión 2 dice dentro del archivo cuántas capas y canales tiene, y la BatchNorm va fundida en las convoluciones; la cámara sigue leyendo también la versión 1 (NumPy).
