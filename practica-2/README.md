# Práctica 2 — Broker MQTT, InfluxDB y Grafana (detector de drones)

Enunciado: `Actividades/Actividad_2_2026-2.pdf`. Igual que la Práctica 1, va orientada al proyecto: en vez del DHT11 la "señal" es la del detector (nivel en la banda del dron y aparatos WiFi). Nube: Azure, sobre la misma red de la Práctica 1.

Diagrama, topics, usuarios y reglas: [`arquitectura.md`](arquitectura.md).

> Estado: código y scripts listos y probados en local (broker con usuarios y ACL, puente, simulador). **Falta correrlos en Azure** y subir el firmware al devkit.

## Qué hay en cada carpeta

| Carpeta | Qué tiene |
| --- | --- |
| `nube/` | `crear_infra_p2.sh` (subred privada, VM de InfluxDB, reglas), `desplegar_p2.sh` (instala todo), `pausar_p2.sh`, `reanudar_p2.sh` |
| `broker/` | Configuración de Mosquitto (`mosquitto.conf`) y permisos por usuario (`acl`) |
| `puente/` | `puente_mqtt.py`: escucha el broker y guarda en InfluxDB + CSV (servicio `puente-mqtt`) |
| `grafana/` | Conexión a InfluxDB, tablero `detector.json`, reglas de alerta y aviso por Telegram |
| `esp32/devkit_mqtt/` | Firmware del devkit: lo mismo de la Práctica 1, pero publicando por MQTT |
| `simulador/` | `simular_mqtt.py`: repite una grabación real por MQTT, para probar sin placas |

## Pasos

Todo en Ubuntu (WSL), con `az login` hecho. Desde `practica-2/nube`.

### 0. (Opcional, recomendado) Bot de Telegram para las alertas

1. En Telegram, habla con **@BotFather** → `/newbot` → ponle un nombre. Te da un **token** (`123456:ABC…`).
2. Escríbele cualquier cosa a tu bot nuevo.
3. Abre `https://api.telegram.org/bot<TOKEN>/getUpdates` en el navegador y copia el número de `"chat":{"id": …}`. Ese es el **chat ID**.

`desplegar_p2.sh` te los pide la primera vez. Si los dejas vacíos, las alertas funcionan en Grafana pero no llegan al celular; para agregarlos después, edita `TELEGRAM_TOKEN` y `TELEGRAM_CHAT_ID` en `nube/secretos_p2.env` y vuelve a correr `bash desplegar_p2.sh`.

### 1. Crear lo nuevo en Azure (~5 min)

```bash
bash crear_infra_p2.sh
```

Crea la subred privada `influx`, su NSG, la VM `vm-influx` sin IP pública y abre 1883 (MQTT) y 3000 (Grafana, solo tus IPs) en `nsg-app`.

### 2. Instalar todo (~10 min)

```bash
bash desplegar_p2.sh
```

Instala Mosquitto con usuarios, InfluxDB en la VM privada, el puente y Grafana con el tablero y las alertas. Al final hace una prueba (publica un latido y lo busca en InfluxDB) e imprime las claves. Se puede repetir sin dañar nada.

### 3. Probar sin placas: simulador

```bash
pip install --break-system-packages "paho-mqtt>=2,<3"     # una vez
cd ../simulador
source ../nube/secretos_p2.env
python3 simular_mqtt.py --clave "$MQTT_CLAVE_JUAN" --dron --vueltas 3
```

`--dron` sube la banda del dron en el tercio del medio de la grabación: debe saltar la alerta en Grafana y en Telegram.

### 4. Ver los datos

- **Grafana:** `http://57.156.64.36:3000`, usuario `admin` (clave en `secretos_p2.env`). Carpeta *Detector* → tablero *Detector de drones · Práctica 2*.
- **MQTT Explorer:** host `57.156.64.36`, puerto `1883`, usuario `juan`. Verás el árbol `detector1/…` en vivo.
- **InfluxDB por dentro** (evidencia de que la base guarda):
  ```bash
  source ../nube/secretos_p2.env
  ssh juan@57.156.64.36 "curl -s -X POST 'http://10.0.3.4:8086/api/v2/query?org=uao' -H 'Authorization: Token $INFLUX_TOKEN' -H 'Content-Type: application/vnd.flux' -d 'from(bucket:\"detector\") |> range(start:-1h) |> filter(fn:(r)=> r._field==\"nivel_banda_dron\") |> count()'"
  ```
- **CSV:** `ssh juan@57.156.64.36 "wc -l ~/practica2/datos/*.csv"`

### 5. La placa real

1. Arduino IDE → Gestor de librerías → instala **PubSubClient** (Nick O'Leary).
2. Copia `esp32/devkit_mqtt/secrets.h.example` como `secrets.h` y pon tu WiFi y `MQTT_CLAVE` = `MQTT_CLAVE_ESP32` de `secretos_p2.env`.
3. Sube `devkit_mqtt.ino` al devkit (ESP32 Dev Module, COM10, BOOT apretado al conectar). El S3 no se toca.
4. En el Monitor Serie debe salir `Conectando al broker ... OK` y `Ventana #... PUBLICADA`.

### 6. Apagar y encender

```bash
bash pausar_p2.sh       # apaga vm-influx, vm-iaot y PostgreSQL
bash reanudar_p2.sh     # enciende vm-influx y vm-iaot (PostgreSQL no hace falta aquí)
```

Desde otro lugar, antes de nada: `bash ../../practica-1/nube/agregar_mi_ip.sh` (actualiza SSH y Grafana).

## Checklist del enunciado

- [ ] Caso de uso definido: detector de drones; la señal es el nivel en 2423–2456 MHz y los aparatos WiFi.
- [ ] Máquina virtual en la nube con broker MQTT (Mosquitto en `vm-iaot`).
- [ ] Broker probado con MQTT Explorer.
- [ ] ESP32 publicando en topics tipo `detector1/nodo-s3/nivel_banda`.
- [ ] Autenticación con usuario y contraseña (`allow_anonymous false` + ACL).
- [ ] Grupo de seguridad revisado (1883 abierto; 3000 y 22 solo desde tus IPs; 8086 solo desde la subred app).
- [ ] Datos en InfluxDB y en CSV.
- [ ] Grafana con tablero, alertas y notificaciones.
- [ ] Base de datos separada en su propia subred (privada).
- [ ] Repositorio con código, README y diagrama.
- [ ] Presentación.

## Límites conocidos

- MQTT va sin cifrar (1883). El usuario y la clave viajan en texto plano; la mejora es TLS en 8883.
- La alerta "Placa sin latidos" solo salta si **no llega ningún** latido; si una de dos placas se cae y la otra sigue, no avisa.
- El puente no guarda en InfluxDB el espectro completo (126 canales), solo su resumen (`nivel_banda_dron`); el espectro completo queda en `ventanas.csv`.
