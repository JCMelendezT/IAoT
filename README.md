# IAoT — Inteligencia Artificial aplicada al IoT

Repositorio de la materia (Especialización en IA, UAO, 2026-2): proyecto final y tres prácticas, todas orientadas al **detector de drones por radiofrecuencia** (ESP32-S3 + nRF24L01+ + sniffer WiFi, 2,4 GHz).

| Qué | Dónde | Nube |
| --- | --- | --- |
| **Proyecto final:** detector de drones con IA en el borde | [`proyecto-detector-drones/`](proyecto-detector-drones/) | — |
| **Práctica 1:** servidor web (FastAPI + PostgreSQL + CSV) | [`practica-1/`](practica-1/) · nube en [`practica-1/nube/`](practica-1/nube/) | Azure |
| **Práctica 2:** broker MQTT, InfluxDB y Grafana con alertas | [`practica-2/`](practica-2/) · arquitectura en [`arquitectura.md`](practica-2/arquitectura.md) | Azure |
| **Práctica 3:** arquitectura serverless (IoT Core, DynamoDB, Lambda, S3) | [`practica-3/`](practica-3/) | AWS |
| **Infraestructura como código** (Terraform) | [`terraform/`](terraform/) (`azure/` y `aws/`) | Azure y AWS |
| Firmware de laboratorio de las placas | [`Lab_ESP32/`](Lab_ESP32/) | — |

Otros documentos: plan por fases [`PLAN.md`](PLAN.md) · estado actual [`00-indice.md`](00-indice.md) · enunciados en [`Actividades/`](Actividades/) · contexto para agentes (OpenCode/Claude) [`AGENTS.md`](AGENTS.md).

## Qué secretos NO están en el repositorio

Git ignora: `secrets.h`, `certs.h`, `.env`, `*.env`, `*.pem`, `terraform.tfvars` y el estado de Terraform. En cada carpeta hay un `.example` o un README que explica cómo generarlos (los scripts y Terraform los crean al desplegar).

## Cómo replicarlo

Guía paso a paso (materiales, cables, Arduino IDE, WSL, servidor y pruebas): https://claude.ai/artifact/1fjUXricZt2E2fco6L6V5p

Resumen rápido:

1. Probar cada placa: `Lab_ESP32/firmware/nrf24_prueba_vida` (S3) y `Lab_ESP32/firmware/devkit_prueba_wifi` (devkit).
2. Windows: WSL en modo `mirrored` y regla de firewall para el puerto 8000.
3. Ubuntu: `cd practica-1/servidor && bash preparar_ubuntu.sh` (una vez) y `bash arrancar.sh`.
4. Sin placas: `python3 simular_nodo.py` repite una grabación real.
5. Placas: `practica-1/esp32-cliente/devkit_pasarela` (devkit) y `practica-1/esp32-cliente/nodo_s3_uart` (S3), unidas por GPIO 17 → GPIO 16 y GND.

Las claves van en `secrets.h` y `.env`, que Git ignora: copia los `.example` y llénalos.
