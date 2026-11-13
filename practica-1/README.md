# Práctica 1. Servidor web para un proyecto IoT

Enunciado: [`docs/curso/actividad-1.md`](../docs/curso/actividad-1.md). El profesor autorizó cambiar el sensor de temperatura y humedad por los datos del detector de drones; el resto del enunciado se mantuvo: servidor web, CSV y base de datos, despliegue en la nube con subredes y repositorio en GitHub. La sustenté el 30 de octubre.

## Qué hace

El ESP32-S3 escucha con el nRF24 y el sniffer WiFi, y le pasa por cable cada ventana de 2 s al ESP32 devkit. El devkit las envía por HTTP al servidor, que las guarda en PostgreSQL y en CSV y las muestra en una página en vivo. Las dos placas hacen falta porque el S3 no puede estar en modo promiscuo saltando de canal y conectado al router a la vez.

Lo que llega al servidor:

- **Ventanas** cada 2 s: tramas y aparatos WiFi, señal, y la ocupación de los 126 canales del nRF24.
- **Eventos** cuando el S3 empieza o termina una alerta.
- **Latidos** de cada placa cada 5 s, para saber que siguen vivas.

Diagramas y tablas de la base: [`arquitectura.md`](arquitectura.md).

## Carpetas

| Carpeta | Contenido |
| --- | --- |
| `servidor/` | API en FastAPI (`app.py`), esquema de la base, página en vivo, simulador y scripts para Ubuntu |
| `esp32-cliente/` | `nodo_s3_uart` (S3), `devkit_pasarela` (devkit) y `devkit_latido` (prueba de conexión) |
| `nube/` | Scripts con la CLI de Azure para crear, desplegar, pausar y borrar todo |
| `diagramas/` | Diagramas de Archify |
| `datos/` | CSV que genera el servidor (fuera de git) |

## Parte 1: en local

El servidor corre en Ubuntu sobre WSL, con la red de WSL en modo *mirrored* y una regla del firewall de Windows para el puerto 8000.

```bash
cd practica-1/servidor
bash preparar_ubuntu.sh      # una sola vez: PostgreSQL, librerías, base iaot y .env
bash arrancar.sh             # muestra la IP del PC y la dirección de la página
python3 simular_nodo.py      # opcional: repite una grabación real sin placas
```

Las placas: `devkit_pasarela` en el devkit (con la IP del PC en su `secrets.h`) y `nodo_s3_uart` en el S3, unidas del GPIO 17 del S3 al GPIO 16 del devkit, más GND.

## Parte 2: en Azure

El mismo servidor en una máquina virtual con IP pública fija, y la base en una subred privada sin acceso desde internet. Los pasos están en [`nube/README.md`](nube/README.md); también se puede crear con Terraform desde `terraform/azure`. La infraestructura ya la borré.

## Cómo comprobé que funcionaba

- En local, con las placas: las ventanas y los latidos de las dos placas llegaban a PostgreSQL y al CSV, y la página se actualizaba sola.
- En Azure: 22 ventanas guardadas en la base, latidos de las dos placas, el CSV creciendo en la VM y la base sin respuesta desde mi PC (como debe ser, por estar en la subred privada).
