# Pinout del nodo detector

Documentar aquí cada conexión real antes de escribir firmware que la use.

Placa asumida: ESP32-S3-DevKitC-1 o clon (confirmar con foto). Los pines del nRF24 usan el bus SPI2 (FSPI) en sus pines nativos: 10, 11, 12, 13. Se evitan los GPIO 0, 3, 45 y 46 (arranque), 19 y 20 (USB), 26 a 37 (memoria flash/PSRAM) y 43 y 44 (UART0).

Estado: **propuesto**, pasar a **confirmado** cuando el nRF24 responda.

| Módulo | Pin del módulo | Pin ESP32-S3 | Notas |
| --- | --- | --- | --- |
| nRF24L01+ | VCC | 3V3 | Con condensador de 10–100 µF entre VCC y GND, lo más cerca posible del módulo |
| nRF24L01+ | GND | GND | |
| nRF24L01+ | CE | GPIO 9 | Propuesto |
| nRF24L01+ | CSN | GPIO 10 | Propuesto (FSPI CS0) |
| nRF24L01+ | SCK | GPIO 12 | Propuesto (FSPI CLK) |
| nRF24L01+ | MOSI | GPIO 11 | Propuesto (FSPI D) |
| nRF24L01+ | MISO | GPIO 13 | Propuesto (FSPI Q) |
| nRF24L01+ | IRQ | Sin conectar | No se necesita para el escáner |
| Pantalla RGB | _pendiente_ | _pendiente_ | |
| Parlante | _pendiente_ | _pendiente_ | |
| Teclado matricial | _pendiente_ | _pendiente_ | |
