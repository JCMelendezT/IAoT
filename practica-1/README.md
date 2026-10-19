# Práctica 1 — Servidor web para un proyecto IoT (orientada al detector de drones)

Enunciado: `../Actividades/Actividad 1 2026-2 (1).md`.

## Caso de uso propuesto

**Nodo de vigilancia RF con monitoreo ambiental.** El nodo del detector de drones se instala en exteriores o semiexteriores, donde temperatura y humedad afectan la electrónica y la estabilidad del nRF24. Se cumple el requisito del enunciado (monitorear temperatura y humedad con DHT) y a la vez el servidor sirve al proyecto:

- **Lecturas ambientales** (DHT11/22): temperatura y humedad cada N segundos.
- **Eventos de detección** del proyecto final: clase, probabilidad, marca de tiempo y latido (heartbeat) del nodo. Se envían como telemetría; la decisión de alerta sigue ocurriendo en el borde, sin depender de la nube.

## Arquitectura

```
ESP32 (DHT) ─┐
             ├─ HTTP POST JSON ─▶ API (FastAPI) ─▶ CSV + base de datos
ESP32-S3 ────┘   /api/lecturas      /api/eventos
```

## Parte 1 — Local (Ubuntu)

- [ ] Aplicación web que reciba y valide lecturas y eventos.
- [ ] Almacenar cada registro en CSV (`datos/`) y en base de datos.
- [ ] Firmware del ESP32 que envíe datos al servidor (`esp32-dht/`).
- [ ] Verificar en el archivo CSV y en la base que los datos medidos llegan completos.

## Parte 2 — Nube (AWS)

- [ ] Máquina virtual con la aplicación en subred **pública**, con **IP elástica**.
- [ ] Base de datos en subred **privada**.
- [ ] Grupos de seguridad: solo el puerto necesario hacia la aplicación; la base solo accesible desde el servidor.
- [ ] Repetir la prueba del ESP32 contra la IP elástica.

## Entrega

- [ ] Repositorio en GitHub (un integrante comparte el enlace).
- [ ] Presentación a los compañeros (material en `entrega/`).

## Criterios de evaluación (del enunciado)

Funcionalidad (datos verificables en CSV/BD), comunicación estable del dispositivo, y servidor accesible de forma remota configurado para recibir y almacenar los datos.
