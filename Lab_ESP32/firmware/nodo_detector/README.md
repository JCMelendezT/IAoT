# nodo_detector — el modelo corre dentro del ESP32-S3

Escucha igual que `nodo_captura` (nRF24 + WiFi en modo promiscuo), pero en vez de mandar los datos al PC, cada 2 s calcula los 28 rasgos, usa el modelo y dice **fondo**, **bluetooth** o **dron**. Si sale dron en 2 ventanas seguidas, enciende la **alerta**. Solo escucha: no transmite nada.

## Cómo subirlo

1. Abre `nodo_detector.ino` en Arduino IDE (los dos `.h` de la carpeta se abren solos).
2. Herramientas: placa **ESP32S3 Dev Module**, **USB CDC On Boot: Enabled**, el puerto del conector **USB** del S3 (no el COM).
3. **Subir**. Si se queda en "Connecting...", mantén BOOT.
4. Abre el **Monitor Serie a 115200**.

Debe salir:

```
# nodo_detector: arrancando
# Modelo: 28 rasgos, clases fondo / bluetooth / dron
# nRF24 OK
# WiFi OK (salto de canales 1-13)
# Listo: una linea cada 2 s
[   1] fondo       fondo  97 % | bluetooth   2 % | dron   1 %   (60 us)
...
>>> ALERTA: DRON DETECTADO <<<
```

LED RGB de la placa: verde = fondo, azul = bluetooth, amarillo = una ventana de dron, **rojo = alerta**, morado = el nRF24 no responde.

## Pantalla táctica y voz de alerta (opcional)

Con la pantalla TFT de 1,8" y el módulo ISD1820 el nodo se vuelve una consola: arranca con un chequeo de cada sistema, muestra la cascada del espectro en vivo con la banda del dron marcada, la probabilidad de dron y el estado (DESPEJADO, BLUETOOTH, POSIBLE DRON). Con la alerta, el recuadro parpadea en rojo con "AMENAZA: DRON" y el parlante dice el mensaje que grabaste, que se repite cada 12 s mientras dure.

1. Instala en el Gestor de librerías: **Adafruit ST7735 and ST7789** (acepta instalar también Adafruit GFX y BusIO).
2. Conecta según `Lab_ESP32/PINOUT.md` (pantalla: SCK 4, SDA 5, A0 6, RESET 7, CS 15, LED y VCC a 3V3; voz: P-E 16).
3. Graba el mensaje: mantén presionado **REC** en el ISD1820 y habla cerca del micrófono (hasta 10 s), por ejemplo "Alerta. Dron detectado." Pruébalo con el botón PLAYE.
4. Sube el sketch. Si no tienes la pantalla o el módulo, pon `USAR_PANTALLA_Y_VOZ` en `0` y funciona como antes.

Si los colores salen invertidos o hay una franja de basura en un borde, cambia `TFT_TIPO` en `pantalla_tactica.h` por `INITR_GREENTAB` o `INITR_REDTAB`.

## Archivos

| Archivo | Qué es |
| --- | --- |
| `nodo_detector.ino` | Lectura de los dos sensores, ventanas de 2 s, alerta y LED |
| `rasgos_detector.h` | Cálculo de rasgos, idéntico a `proyecto-detector-drones/ml/rasgos.py` |
| `modelo_detector.h` | Modelo (lo genera `entrenar.py`; no se edita a mano) |
| `pantalla_tactica.h` | Pantalla TFT y voz de alerta (se activa con `USAR_PANTALLA_Y_VOZ`) |
| `prueba_pc/` | Prueba en el PC: pasa las grabaciones por el mismo código C++ y lo compara con Python |

Si reentrenas, `entrenar.py` deja aquí el `modelo_detector.h` nuevo: basta con volver a subir el sketch.

## Pines

nRF24: CE=9, CSN=10, MOSI=11, SCK=12, MISO=13 (ver `Lab_ESP32/PINOUT.md`). LED RGB integrado: GPIO 48 (DevKitC-1 v1.0). Si no prende, tu placa puede traerlo en el GPIO 38: cambia `PIN_LED_RGB` a `38` al inicio del sketch. El LED es solo una ayuda: la detección y la alerta salen igual por el Monitor Serie.
