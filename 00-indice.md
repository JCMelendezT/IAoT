# Índice y estado

Última actualización: 2026-10-19

## Entregas

| Entrega | Fecha | Estado |
| --- | --- | --- |
| Práctica 1, parte 1 (servidor local Ubuntu + ESP32 + CSV y BD) | Sin fecha fija | Sin iniciar |
| Práctica 1, parte 2 (Azure: subred pública/privada, IP pública estática, NSG) | Sin fecha fija | Sin iniciar |
| Práctica 1: repositorio GitHub + presentación | Sin fecha fija | Sin iniciar |
| Proyecto final: detector de drones RF | Sin fecha fija | nRF24 conectado y confirmado; cascada funcionando; Neo 2 detectado a ojo en 2,4 GHz |

## Plan del proyecto final (del doc de diseño)

1. [x] Conectar el nRF24 con condensador y lograr la primera cascada en el PC (2026-10-20: el Neo 2 se ve en 2,4 GHz).
2. [ ] Activar el sniffer WiFi y extraer rasgos por ventana de 2 s.
3. [ ] Grabar las cinco clases en varias sesiones.
4. [ ] Entrenar línea base (bosque aleatorio) y CNN 2D; cuantizar a int8.
5. [ ] Desplegar en la ESP32-S3 y medir detección, falsas alarmas y alcance.

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
