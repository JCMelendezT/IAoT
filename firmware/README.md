# Firmware

Todo está hecho para Arduino IDE con el core ESP32 3.x. Las conexiones están en [`../docs/hardware.md`](../docs/hardware.md).

| Sketch | Placa | Para qué |
| --- | --- | --- |
| [`nodo_detector`](nodo_detector/) | ESP32-S3 | El detector: escucha, decide con el modelo y alerta |
| [`devkit_centinela`](devkit_centinela/) | ESP32 devkit | Pasarela: caja negra, app del celular y envío a AWS |
| [`nodo_captura`](nodo_captura/) | ESP32-S3 | Manda las filas del nRF24 y el resumen WiFi al PC para grabar datos |

En [`pruebas/`](pruebas/) están los sketches con los que fui probando cada pieza:

| Sketch | Qué prueba |
| --- | --- |
| `nrf24_prueba_vida` | Que el nRF24 responde por SPI (escribe y lee un registro) |
| `nrf24_diagnostico` | Revisa si el nRF24 contesta y, si no, ayuda a encontrar el cable malo |
| `nrf24_cascada` | Manda por el puerto serie la ocupación de los 126 canales (la cascada) |
| `wifi_sniffer` | Escucha el WiFi en modo promiscuo y resume cada 2 s quién transmitió |
| `devkit_prueba_wifi` | Que el devkit se conecta al WiFi de la casa |
| `prueba_pantalla` | La pantalla TFT de 1,8" (no logré que funcionara) |

Las prácticas tienen su propio firmware: `practica-1/esp32-cliente/`, `practica-2/esp32/` y `practica-3/esp32/`.
