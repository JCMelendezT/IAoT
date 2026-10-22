# Pinout del nodo detector

Documentar aquí cada conexión real antes de escribir firmware que la use.

Placa (confirmada por foto, 2026-10-19): clon de ESP32-S3-DevKitC-1 con dos USB-C (probable N16R8: PSRAM octal en GPIO 35–37). La fila de pines del lado de 3V3 trae en orden: 3V3, 3V3, RST, 4, 5, 6, 7, 15, 16, 17, 18, 8, 3, 46, 9, 10, 11, 12, 13, 14, 5V, GND, así que todo el nRF24 sale de esa misma fila.

Módulo de radio: **nRF24L01+ PA+LNA** (versión con amplificador y antena SMA externa). Gasta más corriente que el módulo pequeño: el condensador es obligatorio. Para el escáner solo se recibe, así que el amplificador de transmisión (PA) no se usa y el de recepción (LNA) ayuda a oír más lejos. Hay dos módulos: uno se usa, el otro queda de repuesto. Los pines del nRF24 usan el bus SPI2 (FSPI) en sus pines nativos: 10, 11, 12, 13. Se evitan los GPIO 0, 3, 45 y 46 (arranque), 19 y 20 (USB), 26 a 37 (memoria flash/PSRAM) y 43 y 44 (UART0).

Estado: **confirmado** (2026-10-19). Prueba de vida en Arduino IDE (`firmware/nrf24_prueba_vida/`): escritura y lectura de RF_CH correctas, STATUS = 0x0E. La primera vez falló porque el cable de MISO hacía mal contacto: si vuelve a salir todo en 0x00, revisar ese cable primero. Alimentación por la protoboard con condensador de 100 µF / 16 V.

| Módulo | Pin del módulo | Pin ESP32-S3 | Notas |
| --- | --- | --- | --- |
| nRF24L01+ | VCC | 3V3 | Con condensador de 10–100 µF entre VCC y GND, lo más cerca posible del módulo |
| nRF24L01+ | GND | GND | |
| nRF24L01+ | CE | GPIO 9 | Confirmado |
| nRF24L01+ | CSN | GPIO 10 | Confirmado (FSPI CS0) |
| nRF24L01+ | SCK | GPIO 12 | Confirmado (FSPI CLK) |
| nRF24L01+ | MOSI | GPIO 11 | Confirmado (FSPI D) |
| nRF24L01+ | MISO | GPIO 13 | Confirmado (FSPI Q) |
| nRF24L01+ | IRQ | Sin conectar | No se necesita para el escáner |
| Pantalla RGB | _pendiente_ | _pendiente_ | |
| Parlante | _pendiente_ | _pendiente_ | |
| Teclado matricial | _pendiente_ | _pendiente_ | |

## Cable S3 → devkit (Práctica 1)

| ESP32-S3 | ESP32 devkit | Para qué |
|---|---|---|
| GPIO 17 (TX de UART1) | GPIO 16 (RX2) | el S3 manda cada ventana, latido y alerta |
| GND | GND | referencia común |

Velocidad 115200. Las dos placas trabajan a 3,3 V. GPIO 18 del S3 queda reservado (sin conectar).
