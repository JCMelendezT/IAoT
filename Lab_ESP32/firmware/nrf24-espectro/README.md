# nrf24-espectro

Prueba de vida del nRF24L01+ PA+LNA: lee registros por SPI2 y verifica escritura
con RF_CH (0x4C → lectura → restaurar). **No transmite**: la radio queda en
power-down (PWR_UP=0, CE=0), solo se accede a registros.

## Pines (ver `Lab_ESP32/PINOUT.md`)

| Señal | GPIO |
| --- | --- |
| CE | 9 |
| CSN | 10 |
| MOSI | 11 |
| SCK | 12 |
| MISO | 13 |
| IRQ | sin conectar |
| VCC/GND | 3V3/GND con condensador 10–100 µF en el módulo |

SPI2 (FSPI), modo 0, 1 MHz (cables Dupont de 20 cm).

## Cómo flashear

```powershell
. C:\Espressif\tools\Microsoft.v6.1.PowerShell_profile.ps1   # activa ESP-IDF v6.1
idf.py set-target esp32s3
idf.py build
idf.py -p COM11 flash monitor
```

Salida esperada: valores por defecto del nRF24 (CONFIG=0x08, EN_AA=0x3F,
SETUP_AW=0x03, RF_CH=0x02, RF_SETUP=0x0E, STATUS=0x0E) y `PRUEBA DE VIDA: PASO`.
