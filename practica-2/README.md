# Práctica 2. Broker MQTT, InfluxDB y Grafana

Enunciado: [`docs/curso/actividad-2.pdf`](../docs/curso/actividad-2.pdf). Igual que en la Práctica 1, en lugar del DHT11 la señal es la del detector: el nivel de energía en la banda del dron (2423 a 2456 MHz) y los aparatos WiFi. Montada sobre la misma red de Azure de la Práctica 1; la sustenté el 31 de octubre y después borré la infraestructura.

Arquitectura, topics, usuarios y reglas de seguridad: [`arquitectura.md`](arquitectura.md). Diagrama de Archify: [`diagramas/arquitectura-p2.html`](diagramas/arquitectura-p2.html).

## Piezas

| Carpeta | Contenido |
| --- | --- |
| `broker/` | Configuración de Mosquitto y permisos por usuario (ACL) |
| `puente/` | `puente_mqtt.py`: escucha el broker y guarda en InfluxDB y en CSV |
| `grafana/` | Conexión a InfluxDB, el tablero, las reglas de alerta y el aviso por Telegram |
| `esp32/devkit_mqtt/` | El firmware del devkit de la Práctica 1, publicando por MQTT |
| `simulador/` | `simular_mqtt.py`: repite una grabación real por MQTT, para probar sin placas |
| `nube/` | Scripts para crear la parte nueva en Azure, instalar todo, pausar y reanudar |

Mosquitto, el puente y Grafana van en la VM pública de la Práctica 1. InfluxDB va en una VM aparte, sin IP pública, en la subred privada `influx` (10.0.3.0/24). El enunciado pedía la base en una subred pública; la dejé privada a propósito.

## Pasos

En Ubuntu (WSL), con `az login` hecho y desde `practica-2/nube`:

```bash
bash crear_infra_p2.sh       # subred privada, VM de InfluxDB y reglas (unos 5 min)
bash desplegar_p2.sh         # instala broker, InfluxDB, puente y Grafana; al final hace una prueba
```

`desplegar_p2.sh` pide la primera vez el token y el chat de un bot de Telegram para las alertas (se crea con @BotFather). Si se dejan vacíos, las alertas se ven en Grafana pero no llegan al celular. Las claves quedan en `nube/secretos_p2.env`, fuera de git.

Para probar sin placas:

```bash
pip install --break-system-packages "paho-mqtt>=2,<3"
cd ../simulador
source ../nube/secretos_p2.env
python3 simular_mqtt.py --host <IP pública de la VM> --clave "$MQTT_CLAVE_JUAN" --dron --vueltas 3
```

Con `--dron` sube la banda del dron en el tercio central de la grabación y debe saltar la alerta.

Con las placas: copiar `esp32/devkit_mqtt/secrets.h.example` como `secrets.h`, poner el WiFi, la IP de la VM y la clave `MQTT_CLAVE_ESP32`, instalar PubSubClient y subirlo al devkit. El S3 no cambia.

## Dónde se ven los datos

- **Grafana** en `http://<IP pública>:3000`, usuario `admin`, tablero "Detector de drones".
- **MQTT Explorer** contra la misma IP, puerto 1883, para ver el árbol `detector1/...` en vivo.
- **InfluxDB** por SSH desde la VM pública (no tiene IP propia):
  ```bash
  ssh juan@<IP pública> "curl -s -X POST 'http://10.0.3.4:8086/api/v2/query?org=uao' -H 'Authorization: Token $INFLUX_TOKEN' -H 'Content-Type: application/vnd.flux' -d 'from(bucket:\"detector\") |> range(start:-1h) |> count()'"
  ```
- **CSV** en `~/practica2/datos/` de la VM.

`pausar_p2.sh` y `reanudar_p2.sh` apagan y encienden las máquinas. Desde otro lugar hay que agregar primero la IP nueva con `practica-1/nube/agregar_mi_ip.sh`.

## Límites

- MQTT va sin cifrar por el 1883; el usuario y la clave viajan en texto plano. La mejora sería TLS en el 8883.
- La alerta de "placa sin latidos" solo salta si no llega ninguno; si se cae una de las dos placas y la otra sigue, no avisa.
- InfluxDB guarda el resumen del espectro (`nivel_banda_dron`), no los 126 canales; esos quedan en el CSV.
