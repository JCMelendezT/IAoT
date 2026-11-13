# nodo_detector (ESP32-S3)

El detector. Escucha con el nRF24 y con el WiFi del ESP32 en modo promiscuo, calcula los rasgos de cada ventana de 2 s y el modelo decide: fondo, bluetooth o dron. Con dos ventanas seguidas de dron enciende la alerta. Solo recibe; no transmite nada.

## Subirlo

1. Abrir `nodo_detector.ino` en Arduino IDE.
2. Placa **ESP32S3 Dev Module**, **USB CDC On Boot: Enabled**, y el puerto del conector USB del S3 (no el COM).
3. Subir y abrir el Monitor Serie a 115200.

Al arrancar suena el parlante una vez y luego sale una línea cada 2 s:

```
# Modelo de fabrica: 19 rasgos, clases fondo / bluetooth / dron
# nRF24 OK
[  12] fondo       fondo  97 % | bluetooth   2 % | dron   1 %
>>> ALERTA: DRON DETECTADO <<<
```

El LED de la placa queda verde con fondo, azul con Bluetooth, amarillo con una sola ventana de dron y rojo durante la alerta. Morado quiere decir que el nRF24 no respondió.

## Opciones al inicio del sketch

| Opción | Por defecto | Qué hace |
| --- | --- | --- |
| `USAR_VOZ` | 1 | Hace sonar el ISD1820 al arrancar, al empezar la alerta y cada 5 s mientras dure |
| `USAR_PASARELA` | 1 | Manda cada ventana al devkit por el GPIO 17 y escucha sus órdenes por el GPIO 18 |
| `USAR_PANTALLA` | 0 | La pantalla TFT; desactivada porque no funcionó |

## Archivos

| Archivo | Qué es |
| --- | --- |
| `rasgos_detector.h` | Cálculo de los rasgos; hace exactamente lo mismo que `detector/ml/rasgos.py` |
| `modelo_detector.h` | El modelo de fábrica; lo genera `entrenar.py` |
| `modelo_activo.h` | El modelo en uso, que puede llegar desde la app, y los ajustes guardados |
| `voz_alerta.h` | Manejo del ISD1820 |
| `pantalla_tactica.h` | Código de la pantalla, sin usar |
| `prueba_pc/` | Compila el cálculo de rasgos en el PC y lo compara con Python |

## Lo que viaja por el cable

Hacia el devkit, por cada ventana: una línea `V` con el resumen del WiFi y el espectro (el formato de la Práctica 1), una `D` con la decisión del modelo, una `R` con los 35 rasgos para la caja negra y, cuando empieza o termina una alerta, una `E`.

Desde el devkit llegan órdenes de la app: `M,...` trae un modelo nuevo (con un hash al final; si no coincide, se rechaza), `X` vuelve al modelo de fábrica, `C,clave,valor` cambia un ajuste (`alerta`, `apagar`, `voz`, `repetir`, `probar_voz`) y `Q` pide el estado. El modelo y los ajustes quedan guardados aunque se apague la placa.

## Comprobar contra Python

```bash
cd prueba_pc
g++ -O2 -I.. prueba_rasgos.cpp -o prueba_rasgos
./prueba_rasgos ../../../detector/captura-datos/raw/20261110_*.txt > c_ventanas.csv
python comparar.py c_ventanas.csv
```

Con las grabaciones del 10 de noviembre las 1134 ventanas dan la misma clase en los dos.
