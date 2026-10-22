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
ESP32-S3 (nodo: nRF24 + sniffer) ── UART ──▶ ESP32 devkit (pasarela WiFi)
                                               │ HTTP POST JSON
                                               ▼
                         API FastAPI (Ubuntu WSL, puerto 8000) ──▶ CSV (datos/) + PostgreSQL
                           /api/ventanas  /api/eventos  /api/latido
                           /  (página en vivo)   /api/resumen   /docs
```

El S3 no puede escuchar en modo promiscuo saltando de canal y a la vez estar conectado al router; por eso el devkit hace de pasarela.

## Cómo encender el servidor (Ubuntu en WSL)

Desde Ubuntu, en `/mnt/c/Users/juanc/Desktop/IAoT/practica-1/servidor`:

1. Una sola vez: `bash preparar_ubuntu.sh` (instala PostgreSQL y las librerías, crea la base `iaot` y el archivo `.env`).
2. Cada vez: `bash arrancar.sh`. Muestra la IP del PC y la dirección de la página.
3. Sin placas: `python3 simular_nodo.py` repite una grabación real contra el servidor.

Requisito de Windows (ya hecho): WSL en modo `mirrored` y regla de firewall Hyper-V para el puerto 8000.

## Parte 1 — Local (Ubuntu)

- [x] Aplicación web que reciba y valide ventanas, eventos y latidos (`servidor/app.py`).
- [x] Almacenar cada registro en CSV (`datos/`) y en PostgreSQL (probado con el simulador).
- [ ] Firmware cliente que envíe datos al servidor (`esp32-cliente/`).
- [ ] Verificar en el CSV y en la base que los datos medidos llegan completos.

## Parte 2 — Nube (Azure)

El profesor autorizó cualquier proveedor; se usa Azure for Students. Detalle y cuidados de costo en `../PLAN.md`, Fase 3.


- [ ] Máquina virtual Ubuntu (B1s) con la aplicación en la subred **pública** `app`, con **IP pública estática** (equivale a la IP elástica).
- [ ] PostgreSQL Flexible Server (B1MS) con acceso privado en la subred **privada** `db`.
- [ ] NSG (grupos de seguridad): solo el puerto de la aplicación abierto a internet, SSH solo desde tu IP, y el 5432 de la base solo desde la subred `app`.
- [ ] Presupuesto con alerta en Cost Management.
- [ ] Repetir la prueba del nodo contra la IP pública estática.

## Entrega

- [ ] Repositorio en GitHub (un integrante comparte el enlace).
- [ ] Presentación a los compañeros (material en `entrega/`).
- [ ] Guardar en `entrega/` la autorización del profesor para el cambio de caso de uso.

## Criterios de evaluación (del enunciado)

Funcionalidad (datos verificables en CSV/BD), comunicación estable del dispositivo, y servidor accesible de forma remota configurado para recibir y almacenar los datos.
