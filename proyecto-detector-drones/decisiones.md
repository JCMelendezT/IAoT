# Bitácora de decisiones

Una entrada por decisión, la más reciente arriba. Formato: fecha, decisión, motivo.

## Decididas

- **2026-10-20 — Hallazgo del sniffer WiFi:** con el Neo 2 usando el control en 2,4 GHz no aparece ninguna MAC de DJI (su enlace no es WiFi estándar), pero el ESP32 decodifica un 64 % menos de tramas WiFi y ve un 67 % menos de aparatos mientras el dron está encendido. Esa caída es un rasgo útil para el modelo WiFi. Falta probar el modo solo celular, donde el dron sí debería usar WiFi.
- **2026-10-20 — Las capturas con el DJI Neo 2 se hacen con la transmisión fijada en 2,4 GHz** (DJI Fly → Trans. → Frecuencia), canal automático, 20 MHz. Motivo: en Banda dual o 5,8 GHz el dron no aparece en la cascada del nRF24. Prueba exploratoria: en 2,4 GHz la actividad en 2423–2456 MHz sube de 4 a 9 veces frente al fondo, con pico cerca de 2445 MHz; al pasar a 5,8 GHz vuelve al nivel de fondo. Esto confirma el límite de alcance del diseño (solo 2,4 GHz) y va en el informe.
- **2026-10-19 — El firmware se escribe en Arduino IDE** (sketches en `Lab_ESP32/firmware/`). Juan prefiere Arduino; AGENTS.md no se modifica por ahora.
- **2026-10-19 — Sin fechas fijas en el plan; la Práctica 1 es individual.** Juan avanza a su ritmo y hace todo él mismo.
- **2026-10-19 — Nube de la Práctica 1: Azure** (Azure for Students, ~20 USD de crédito restante). El profesor autorizó cualquier proveedor. Se apoya en los servicios gratis de 12 meses: VM B1s y PostgreSQL Flexible Server B1MS.
- **2026-10-18 — El profesor autorizó cambiar el caso de uso de la Práctica 1** (temperatura y humedad con DHT) por uno orientado al proyecto. Los datos que llegan al servidor son los del nodo detector: rasgos WiFi, resumen del espectro nRF24 y eventos de detección. Se asume que el resto del enunciado se mantiene (servidor web, CSV y base de datos, nube con subredes, GitHub, presentación). Conviene guardar la autorización por escrito en `practica-1/entrega/`.
- **2026-10-18 — Repositorio único `IAoT`** para Práctica 1 y proyecto final, con `AGENTS.md` para que OpenCode tenga el contexto. Motivo: un solo historial en GitHub, que además cumple el requisito de la Práctica 1.
- **2026-10-18 — La Práctica 1 se hace orientada al proyecto**, cumpliendo sus dos partes completas, para avanzar en ambos a la vez.
- **2026-10-17 — Proyecto final: detector de drones por radiofrecuencia.** Se descartaron el radar de presencia con WiFi CSI y el aprendizaje federado entre nodos ESP32. Motivo: relevancia por los ataques con drones en Colombia y viabilidad con el hardware disponible.

## Propuestas por confirmar

- **Nodo sensor + pasarela:** el ESP32-S3 solo escucha y detecta; le pasa los resultados por UART al ESP32 devkit, que está conectado al WiFi y los envía al servidor. Motivo: el S3 no puede estar en modo promiscuo saltando de canal y conectado al router a la vez.
- **Ubuntu local en WSL2** con red en modo *mirrored* para que el ESP32 alcance el servidor. Alternativa: máquina virtual con Ubuntu en red puente.
- **Base de datos en Azure como PostgreSQL Flexible Server** (B1MS, acceso privado en subred delegada). Motivo: entra en los servicios gratis de Azure for Students y evita una segunda VM en la subred privada.
- **Framework del servidor (Práctica 1):** FastAPI (Python), por coherencia con el pipeline de ML. Alternativas permitidas por el enunciado: Flask, Django, Node, PHP, Java.
- **Base de datos:** PostgreSQL en ambos entornos, para que el paso local → Azure (BD en subred privada) no cambie el código. SQLite solo como atajo de arranque.
- **Protocolo ESP32 → servidor:** HTTP POST con JSON. MQTT queda como mejora opcional.

## Pendientes de definir

