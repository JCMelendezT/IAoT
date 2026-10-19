# Bitácora de decisiones

Una entrada por decisión, la más reciente arriba. Formato: fecha, decisión, motivo.

## Decididas

- **2026-10-18 — El profesor autorizó cambiar el caso de uso de la Práctica 1** (temperatura y humedad con DHT) por uno orientado al proyecto. Los datos que llegan al servidor son los del nodo detector: rasgos WiFi, resumen del espectro nRF24 y eventos de detección. Se asume que el resto del enunciado se mantiene (servidor web, CSV y base de datos, AWS con subredes, GitHub, presentación). Conviene guardar la autorización por escrito en `practica-1/entrega/`.
- **2026-10-18 — Repositorio único `IAoT`** para Práctica 1 y proyecto final, con `AGENTS.md` para que OpenCode tenga el contexto. Motivo: un solo historial en GitHub, que además cumple el requisito de la Práctica 1.
- **2026-10-18 — La Práctica 1 se hace orientada al proyecto**, cumpliendo sus dos partes completas, para avanzar en ambos a la vez.
- **2026-10-17 — Proyecto final: detector de drones por radiofrecuencia.** Se descartaron el radar de presencia con WiFi CSI y el aprendizaje federado entre nodos ESP32. Motivo: relevancia por los ataques con drones en Colombia y viabilidad con el hardware disponible.

## Propuestas por confirmar

- **Framework del servidor (Práctica 1):** FastAPI (Python), por coherencia con el pipeline de ML. Alternativas permitidas por el enunciado: Flask, Django, Node, PHP, Java.
- **Base de datos:** PostgreSQL en ambos entornos, para que el paso local → AWS (BD en subred privada) no cambie el código. SQLite solo como atajo de arranque.
- **Protocolo ESP32 → servidor:** HTTP POST con JSON. MQTT queda como mejora opcional.
- **Nube:** AWS (EC2 en subred pública con IP elástica, BD en subred privada, grupos de seguridad). Confirmar que el profesor no acepta Azure si se prefiere.

## Pendientes de definir

- Fechas de entrega de la Práctica 1 y del proyecto final.
- Si la Práctica 1 es individual o en equipo (el enunciado habla de "integrantes").
