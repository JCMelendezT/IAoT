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

## Archivos

| Archivo | Qué es |
| --- | --- |
| `nodo_detector.ino` | Lectura de los dos sensores, ventanas de 2 s, alerta y LED |
| `rasgos_detector.h` | Cálculo de rasgos, idéntico a `proyecto-detector-drones/ml/rasgos.py` |
| `modelo_detector.h` | Modelo (lo genera `entrenar.py`; no se edita a mano) |
| `prueba_pc/` | Prueba en el PC: pasa las grabaciones por el mismo código C++ y lo compara con Python |

Si reentrenas, `entrenar.py` deja aquí el `modelo_detector.h` nuevo: basta con volver a subir el sketch.

## Pines

nRF24: CE=9, CSN=10, MOSI=11, SCK=12, MISO=13 (ver `Lab_ESP32/PINOUT.md`). LED RGB integrado: GPIO 48 (DevKitC-1 v1.0). Si no prende, tu placa puede traerlo en el GPIO 38: cambia `PIN_LED_RGB` a `38` al inicio del sketch. El LED es solo una ayuda: la detección y la alerta salen igual por el Monitor Serie.
