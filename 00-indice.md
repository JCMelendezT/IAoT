# Índice y estado

Última actualización: 2026-10-18

## Entregas

| Entrega | Fecha | Estado |
| --- | --- | --- |
| Práctica 1, parte 1 (servidor local Ubuntu + ESP32 + CSV y BD) | Sin fecha fija | Sin iniciar |
| Práctica 1, parte 2 (Azure: subred pública/privada, IP pública estática, NSG) | Sin fecha fija | Sin iniciar |
| Práctica 1: repositorio GitHub + presentación | Sin fecha fija | Sin iniciar |
| Proyecto final: detector de drones RF | Sin fecha fija | Diseño listo, sin hardware conectado |

## Plan del proyecto final (del doc de diseño)

1. [ ] Conectar el nRF24 con condensador y lograr la primera cascada en el PC.
2. [ ] Activar el sniffer WiFi y extraer rasgos por ventana de 2 s.
3. [ ] Grabar las cinco clases en varias sesiones.
4. [ ] Entrenar línea base (bosque aleatorio) y CNN 2D; cuantizar a int8.
5. [ ] Desplegar en la ESP32-S3 y medir detección, falsas alarmas y alcance.

## Pendientes de entorno

- [ ] Habilitar `SERIAL_MCP_PLUGINS` y verificar que `idf.py` resuelva (OpenCode).
- [ ] Documentar pines reales en `Lab_ESP32/PINOUT.md`.
- [ ] Crear el repositorio remoto en GitHub (privado) y enlazarlo.
- [ ] Reunir: condensador 10–100 µF, cables Dupont, protoboard, cable USB de datos, protectores de hélice, cinta métrica.

## Documentos clave

- **Plan completo, explicado paso a paso:** `PLAN.md`
- Diseño del detector: `Recursos de clase/Detector de drones por radiofrecuencia con IA en el borde.md`
- Enunciado Práctica 1: `Actividades/Actividad 1 2026-2 (1).md`
- Decisiones: `proyecto-detector-drones/decisiones.md`
