# Índice y estado

Última actualización: 2026-11-10

## Entregas

| Entrega | Fecha | Estado |
| --- | --- | --- |
| Práctica 1, parte 1 (servidor local Ubuntu + ESP32 + CSV y BD) | — | Hecha |
| Práctica 1, parte 2 (Azure: subred pública/privada, IP pública estática, NSG) | 2026-10-30 | Sustentada |
| Práctica 2 (MQTT + InfluxDB + Grafana en Azure) | 2026-10-31 | Presentada |
| Práctica 3 (AWS serverless: IoT Core, DynamoDB, Lambda, S3) | 2026-11-05 | Desplegada con Terraform y probada (simulador y dashboard); falta la presentación |
| Terraform (`terraform/azure` y `terraform/aws`) | 2026-11-05 | AWS aplicado y verificado; Azure escrito, sin aplicar |
| Proyecto final: detector de drones RF | Sin fecha fija | Modelo de 3 clases entrenado (99,3 %) y firmware `nodo_detector` listo para subir; falta probarlo en la placa y medir alcance |

## Plan del proyecto final (del doc de diseño)

1. [x] Conectar el nRF24 con condensador y lograr la primera cascada en el PC (2026-10-20: el Neo 2 se ve en 2,4 GHz).
2. [x] Activar el sniffer WiFi y extraer rasgos por ventana de 2 s (`nodo_captura` + `ml/rasgos.py`).
3. [x] Grabar las clases (2026-11-10): una toma larga por clase de fondo, Bluetooth y dron con control (tierra y vuelo). Se dejó fuera `celular-video` y `dron-app` (no detectable).
4. [x] Entrenar (2026-11-10): regresión logística, 99,3 % por bloques de 1 min. Ocupa ~572 bytes, así que no hizo falta CNN ni cuantizar a int8.
5. [ ] Desplegar en la ESP32-S3 y medir detección, falsas alarmas y alcance. Firmware hecho y comprobado en el PC; falta la prueba en la placa.

## Pendientes de entorno

- [x] Verificar que `idf.py` resuelva (2026-10-19): requiere activar `. C:\Espressif\tools\Microsoft.v6.1.PowerShell_profile.ps1`; sin eso no está en PATH. Ver `Lab_ESP32/firmware/smoke-test/README.md`.
- [ ] Habilitar `SERIAL_MCP_PLUGINS` (OpenCode).
- [ ] Documentar pines reales en `Lab_ESP32/PINOUT.md`.
- [ ] Crear el repositorio remoto en GitHub (privado) y enlazarlo.
- [ ] Reunir: condensador 10–100 µF, cables Dupont, protoboard, cable USB de datos, protectores de hélice, cinta métrica.

## Documentos clave

- **Plan completo, explicado paso a paso:** `PLAN.md`
- Diseño del detector: `Recursos de clase/Detector de drones por radiofrecuencia con IA en el borde.md`
- Enunciado Práctica 1: `Actividades/Actividad 1 2026-2 (1).md`
- Decisiones: `proyecto-detector-drones/decisiones.md`
