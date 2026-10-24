# IAoT — Inteligencia Artificial aplicada al IoT

Repositorio de la materia (Especialización en IA, UAO, 2026-2): práctica 1 y proyecto final.

- **Proyecto final:** detector de drones por radiofrecuencia con IA en ESP32-S3 → `proyecto-detector-drones/`
- **Práctica 1:** servidor web para IoT, orientada al proyecto → `practica-1/`
- **Arquitectura (diagramas):** `practica-1/arquitectura.md`
- **Nube (Azure, parte 2):** `practica-1/nube/`
- **Plan por fases:** `PLAN.md`
- **Estado actual:** `00-indice.md`
- **Contexto para agentes (OpenCode/Claude):** `AGENTS.md`

## Cómo replicarlo

Guía paso a paso (materiales, cables, Arduino IDE, WSL, servidor y pruebas): https://claude.ai/artifact/1fjUXricZt2E2fco6L6V5p

Resumen rápido:

1. Probar cada placa: `Lab_ESP32/firmware/nrf24_prueba_vida` (S3) y `Lab_ESP32/firmware/devkit_prueba_wifi` (devkit).
2. Windows: WSL en modo `mirrored` y regla de firewall para el puerto 8000.
3. Ubuntu: `cd practica-1/servidor && bash preparar_ubuntu.sh` (una vez) y `bash arrancar.sh`.
4. Sin placas: `python3 simular_nodo.py` repite una grabación real.
5. Placas: `practica-1/esp32-cliente/devkit_pasarela` (devkit) y `practica-1/esp32-cliente/nodo_s3_uart` (S3), unidas por GPIO 17 → GPIO 16 y GND.

Las claves van en `secrets.h` y `.env`, que Git ignora: copia los `.example` y llénalos.
