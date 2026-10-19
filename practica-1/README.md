# Práctica 1 — Servidor web para un proyecto IoT (orientada al detector de drones)

Enunciado: `../Actividades/Actividad 1 2026-2 (1).md`.

El profesor autorizó cambiar el caso de uso original (temperatura y humedad con DHT) por uno orientado al proyecto final. Se asume que lo demás del enunciado se mantiene: servidor web, almacenamiento en CSV y base de datos, despliegue en la nube con subredes, repositorio en GitHub y presentación.

## Caso de uso

**Telemetría del nodo detector de drones.** El dispositivo inteligente es el nodo del proyecto final. Mide con sus propios sensores y envía los datos en tiempo real al servidor:

- **Ventanas de rasgos WiFi** (cada 2 s): tasa de tramas, tamaños, tiempos entre tramas, RSSI.
- **Resumen del espectro nRF24**: canal de mayor energía y ocupación de los 126 canales por barrido.
- **Eventos de detección**: clase, probabilidad, marca de tiempo.
- **Latido (heartbeat)** del nodo para saber que sigue vivo.

La alerta sigue decidiéndose en el borde, sin depender de la nube. El servidor registra, almacena y permite revisar lo que el nodo ve. Esto además deja un historial útil para calcular falsas alarmas por hora en el proyecto final.

## Arquitectura

```
ESP32-S3 (nodo detector) ── HTTP POST JSON ──▶ API (FastAPI) ──▶ CSV + base de datos
                           /api/ventanas
                           /api/eventos
                           /api/latido
```

La ESP32 devkit queda opcional como segundo cliente de pruebas.

## Parte 1 — Local (Ubuntu)

- [ ] Aplicación web que reciba y valide ventanas, eventos y latidos.
- [ ] Almacenar cada registro en CSV (`datos/`) y en base de datos.
- [ ] Firmware cliente que envíe datos al servidor (`esp32-cliente/`).
- [ ] Verificar en el CSV y en la base que los datos medidos llegan completos.

## Parte 2 — Nube (AWS)

- [ ] Máquina virtual con la aplicación en subred **pública**, con **IP elástica**.
- [ ] Base de datos en subred **privada**.
- [ ] Grupos de seguridad: solo el puerto necesario hacia la aplicación; la base solo accesible desde el servidor.
- [ ] Repetir la prueba del nodo contra la IP elástica.

## Entrega

- [ ] Repositorio en GitHub (un integrante comparte el enlace).
- [ ] Presentación a los compañeros (material en `entrega/`).
- [ ] Guardar en `entrega/` la autorización del profesor para el cambio de caso de uso.

## Criterios de evaluación (del enunciado)

Funcionalidad (datos verificables en CSV/BD), comunicación estable del dispositivo, y servidor accesible de forma remota configurado para recibir y almacenar los datos.
