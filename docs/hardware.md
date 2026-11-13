# Hardware y conexiones

Cada conexión se anota aquí antes de usarla en un sketch.

## Placas

**ESP32-S3.** Clon de la DevKitC-1 con dos USB-C, probablemente N16R8 (la PSRAM octal ocupa los GPIO 35 a 37). Para programarla uso el conector marcado USB, con *USB CDC On Boot: Enabled*; el otro conector (COM) no muestra lo que imprime el sketch. La fila de pines del lado de 3V3 trae, en orden: 3V3, 3V3, RST, 4, 5, 6, 7, 15, 16, 17, 18, 8, 3, 46, 9, 10, 11, 12, 13, 14, 5V y GND. Casi todo sale de esa fila.

Pines que evito: 0, 3, 45 y 46 (arranque), 19 y 20 (USB), 26 a 37 (flash y PSRAM) y 43 y 44 (UART0).

**ESP32 devkit.** Placa común con chip USB CP210x. Para subirle programas hay que mantener BOOT mientras dice "Connecting...".

**Radio.** nRF24L01+ PA+LNA, la versión con antena SMA. Solo recibe: el amplificador de transmisión no se usa y el de recepción ayuda a oír más lejos. Gasta más que el módulo pequeño, así que lleva un condensador de 100 µF / 16 V entre VCC y GND, pegado al módulo. Tengo dos; uno queda de repuesto.

## ESP32-S3 (nodo detector)

| Pieza | Pin de la pieza | Pin del S3 | Notas |
| --- | --- | --- | --- |
| nRF24L01+ | VCC / GND | 3V3 / GND | Con el condensador de 100 µF |
| nRF24L01+ | CE | 9 | |
| nRF24L01+ | CSN | 10 | Bus FSPI en sus pines nativos |
| nRF24L01+ | MOSI | 11 | |
| nRF24L01+ | SCK | 12 | |
| nRF24L01+ | MISO | 13 | Si todo se lee como 0x00, revisar primero este cable |
| nRF24L01+ | IRQ | sin conectar | |
| ISD1820 (sonido) | VCC / GND | 3V3 / GND | Columna izquierda del conector J1 |
| ISD1820 | P-E | 16 | Un pulso de 80 ms reproduce todo lo grabado |
| ISD1820 | FT, P-L, REC | sin conectar | Quitar los puentes FT y P-E del módulo |
| Parlante 8 Ω 0,5 W | | conector SP del ISD1820 | No va al ESP32 |
| LED RGB de la placa | | 48 | Viene en la placa. En la versión v1.1 puede ser el 38 |
| Devkit | RX2 (GPIO 16) | 17 | Datos del detector |
| Devkit | TX2 (GPIO 17) | 18 | Órdenes desde la app (modelo y ajustes) |
| Devkit | GND | GND | Obligatorio para que el cable funcione |

El cable entre las dos placas va a 115200 baudios; ambas trabajan a 3,3 V.

El sonido se graba en el ISD1820 una sola vez: se mantiene REC y se habla o se pone un pitido junto al micrófono (máximo 10 s).

## Pantalla que no funcionó

Probé una TFT de 1,8" (ST7735, 128x160, marcada VER 1.0) en un bus SPI aparte: SCK 4, SDA 5, DC 6, RES 7, CS 15, BL a 3V3. El módulo trae regulador en VCC, así que la alimenté con 5 V. Siempre quedó en blanco: cambié pines, bajé la velocidad a 10 MHz, probé SPI por software y los cuatro modos de arranque del controlador. El código quedó en `firmware/nodo_detector/pantalla_tactica.h` y `firmware/pruebas/prueba_pantalla/`, desactivado, por si consigo otra pantalla.

## Pines libres

En la fila de 3V3 quedan libres 4, 5, 6, 7, 15, 8 y 14 (los de la pantalla vuelven a estar disponibles).
