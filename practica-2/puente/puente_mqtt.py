#!/usr/bin/env python3
"""
Puente MQTT -> InfluxDB + CSV (Practica 2, detector de drones).

Se suscribe a todo lo que publica el detector en el broker (detector1/#) y guarda
cada mensaje dos veces:
  - en InfluxDB (base de series de tiempo, en la subred privada), y
  - en archivos CSV (carpeta DATOS_DIR).

Topics que entiende (los publica el devkit o el simulador):
  detector1/<nodo>/ventana   JSON con el resumen de una ventana de escucha
  detector1/<nodo>/latido    JSON {"contador", "ms", "rssi"?}
  detector1/<nodo>/evento    JSON {"ms", "tipo", "detalle"?}
  detector1/<nodo>/nivel_banda, .../aparatos, .../estado   valores sueltos (solo para ver
                                                          en MQTT Explorer; no se guardan)

Configuracion: archivo .env junto a este script (lo escribe desplegar_p2.sh).
Arranque manual:  python3 puente_mqtt.py
"""
from __future__ import annotations

import csv
import json
import signal
import sys
import threading
import time
import urllib.error
import urllib.parse
import urllib.request
from datetime import datetime, timezone
from pathlib import Path

import paho.mqtt.client as mqtt

AQUI = Path(__file__).resolve().parent

# Banda donde aparecio el DJI Neo 2 con control (2423..2456 MHz) = columnas 23..56
# del espectro de 126 canales. Misma regla que la Practica 1.
BANDA_DRON = (23, 57)

CAMPOS_VENTANA = ["ms", "ventana", "canal", "tramas", "aparatos", "mgmt", "datos", "tam_prom",
                  "rssi_prom", "rssi_max", "tramas_debiles", "aparatos_fuertes", "tramas_dji",
                  "rssi_max_dji"]
DECIMALES = {"tam_prom", "rssi_prom"}


# ---------------------------------------------------------------- configuracion
def leer_env(ruta: Path) -> dict:
    valores = {}
    if ruta.exists():
        for linea in ruta.read_text(encoding="utf-8").splitlines():
            linea = linea.strip()
            if linea and not linea.startswith("#") and "=" in linea:
                k, v = linea.split("=", 1)
                valores[k.strip()] = v.strip().strip('"').strip("'")
    return valores


ENV = leer_env(AQUI / ".env")
MQTT_HOST = ENV.get("MQTT_HOST", "localhost")
MQTT_PUERTO = int(ENV.get("MQTT_PUERTO", "1883"))
MQTT_USUARIO = ENV.get("MQTT_USUARIO", "puente")
MQTT_CLAVE = ENV.get("MQTT_CLAVE", "")
MQTT_TOPIC = ENV.get("MQTT_TOPIC", "detector1/#")
INFLUX_URL = ENV.get("INFLUX_URL", "http://10.0.3.4:8086").rstrip("/")
INFLUX_ORG = ENV.get("INFLUX_ORG", "uao")
INFLUX_BUCKET = ENV.get("INFLUX_BUCKET", "detector")
INFLUX_TOKEN = ENV.get("INFLUX_TOKEN", "")
DATOS_DIR = Path(ENV.get("DATOS_DIR", str(AQUI.parent / "datos")))
UMBRAL_BANDA = float(ENV.get("UMBRAL_BANDA", "2.5"))


def log(msg: str):
    print(f"[{datetime.now().strftime('%H:%M:%S')}] {msg}", flush=True)


# ---------------------------------------------------------------- reglas
def nivel_banda(espectro):
    if not isinstance(espectro, str) or len(espectro) != 126 or not espectro.isdigit():
        return None
    trozo = espectro[BANDA_DRON[0]:BANDA_DRON[1]]
    return round(sum(int(c) for c in trozo) / len(trozo), 2)


def revisar_alerta(v: dict, nivel):
    motivos = []
    if v.get("tramas_dji", 0) > 0:
        motivos.append(f"{v['tramas_dji']} tramas de un aparato DJI")
    if nivel is not None and nivel >= UMBRAL_BANDA:
        motivos.append(f"banda del dron ocupada (nivel {nivel} >= {UMBRAL_BANDA})")
    return bool(motivos), "; ".join(motivos)


# ---------------------------------------------------------------- InfluxDB (line protocol)
def _esc_clave(s: str) -> str:
    return str(s).replace("\\", "\\\\").replace(",", "\\,").replace("=", "\\=").replace(" ", "\\ ")


def _campo(v) -> str:
    if isinstance(v, bool):
        return "true" if v else "false"
    if isinstance(v, int):
        return f"{v}i"
    if isinstance(v, float):
        return repr(v)
    return '"' + str(v).replace("\\", "\\\\").replace('"', '\\"') + '"'


def linea_influx(medida: str, etiquetas: dict, campos: dict, ts_ns: int) -> str:
    tags = ",".join(f"{_esc_clave(k)}={_esc_clave(v)}" for k, v in etiquetas.items() if v not in (None, ""))
    fields = ",".join(f"{_esc_clave(k)}={_campo(v)}" for k, v in campos.items() if v is not None)
    cabeza = _esc_clave(medida) + ("," + tags if tags else "")
    return f"{cabeza} {fields} {ts_ns}"


def escribir_influx(lineas: str) -> bool:
    url = f"{INFLUX_URL}/api/v2/write?" + urllib.parse.urlencode(
        {"org": INFLUX_ORG, "bucket": INFLUX_BUCKET, "precision": "ns"})
    pet = urllib.request.Request(url, data=lineas.encode(), method="POST", headers={
        "Authorization": f"Token {INFLUX_TOKEN}", "Content-Type": "text/plain; charset=utf-8"})
    try:
        with urllib.request.urlopen(pet, timeout=5) as r:
            return 200 <= r.status < 300
    except urllib.error.HTTPError as e:
        log(f"   !! InfluxDB respondio {e.code}: {e.read().decode(errors='replace')[:200]}")
    except (urllib.error.URLError, OSError) as e:
        log(f"   !! InfluxDB no responde: {e}")
    return False


# ---------------------------------------------------------------- CSV
_candado = threading.Lock()
COLUMNAS_CSV = {
    "ventanas": ["recibido_en", "nodo"] + CAMPOS_VENTANA + ["espectro", "nivel_banda_dron", "alerta", "motivo_alerta"],
    "latidos": ["recibido_en", "nodo", "contador", "ms", "rssi"],
    "eventos": ["recibido_en", "nodo", "ms", "tipo", "detalle"],
}


def guardar_csv(nombre: str, fila: dict):
    DATOS_DIR.mkdir(parents=True, exist_ok=True)
    ruta = DATOS_DIR / f"{nombre}.csv"
    with _candado:
        nuevo = not ruta.exists()
        with ruta.open("a", newline="", encoding="utf-8") as f:
            w = csv.DictWriter(f, fieldnames=COLUMNAS_CSV[nombre], extrasaction="ignore")
            if nuevo:
                w.writeheader()
            w.writerow(fila)


# ---------------------------------------------------------------- mensajes
contadores = {"ventanas": 0, "latidos": 0, "eventos": 0, "influx_fallos": 0, "descartados": 0}


def entero(v):
    return int(v) if isinstance(v, (int, float)) and not isinstance(v, bool) else None


def procesar(topic: str, payload: bytes):
    partes = topic.split("/")
    if len(partes) != 3:
        return
    _, nodo, tipo = partes
    if tipo not in ("ventana", "latido", "evento"):
        return  # nivel_banda, aparatos, estado: solo para ver en vivo
    try:
        datos = json.loads(payload.decode("utf-8"))
        if not isinstance(datos, dict):
            raise ValueError("no es un objeto JSON")
    except (UnicodeDecodeError, ValueError) as e:
        contadores["descartados"] += 1
        log(f"   ?? {topic}: mensaje no valido ({e}), lo ignoro")
        return

    ahora = datetime.now(timezone.utc)
    ts_ns = time.time_ns()
    recibido = ahora.astimezone().isoformat(timespec="seconds")

    if tipo == "ventana":
        v = {}
        for k in CAMPOS_VENTANA:
            x = datos.get(k)
            if not isinstance(x, (int, float)) or isinstance(x, bool):
                contadores["descartados"] += 1
                log(f"   ?? {topic}: falta o no es numero '{k}', ignoro la ventana")
                return
            v[k] = float(x) if k in DECIMALES else int(x)
        espectro = datos.get("espectro")
        nivel = nivel_banda(espectro)
        alerta, motivo = revisar_alerta(v, nivel)
        campos = dict(v) | {"nivel_banda_dron": nivel, "alerta": alerta}
        if motivo:
            campos["motivo_alerta"] = motivo
        ok = escribir_influx(linea_influx("ventanas", {"nodo": nodo}, campos, ts_ns))
        guardar_csv("ventanas", {"recibido_en": recibido, "nodo": nodo, **v,
                                 "espectro": espectro if nivel is not None else "",
                                 "nivel_banda_dron": nivel, "alerta": alerta, "motivo_alerta": motivo})
        contadores["ventanas"] += 1
        aviso = f"  <<< POSIBLE DRON: {motivo}" if alerta else ""
        log(f"ventana {nodo} #{v['ventana']}  {v['tramas']} tramas, {v['aparatos']} aparatos, "
            f"banda {nivel if nivel is not None else '-'}{'' if ok else '  (sin Influx)'}{aviso}")

    elif tipo == "latido":
        campos = {"contador": entero(datos.get("contador")), "ms": entero(datos.get("ms")),
                  "rssi": entero(datos.get("rssi"))}
        if campos["contador"] is None:
            contadores["descartados"] += 1
            return
        ok = escribir_influx(linea_influx("latidos", {"nodo": nodo}, campos, ts_ns))
        guardar_csv("latidos", {"recibido_en": recibido, "nodo": nodo, **campos})
        contadores["latidos"] += 1
        senal = f"senal {campos['rssi']} dBm" if campos["rssi"] is not None else "por cable"
        log(f"latido  {nodo} #{campos['contador']}  {senal}{'' if ok else '  (sin Influx)'}")

    else:  # evento
        tipo_ev = str(datos.get("tipo", ""))[:40]
        if not tipo_ev:
            contadores["descartados"] += 1
            return
        detalle = str(datos.get("detalle", ""))[:300]
        ms = entero(datos.get("ms"))
        ok = escribir_influx(linea_influx("eventos", {"nodo": nodo, "tipo": tipo_ev},
                                          {"valor": 1, "ms": ms, "detalle": detalle}, ts_ns))
        guardar_csv("eventos", {"recibido_en": recibido, "nodo": nodo, "ms": ms, "tipo": tipo_ev, "detalle": detalle})
        contadores["eventos"] += 1
        log(f"EVENTO  {nodo} {tipo_ev} {detalle}{'' if ok else '  (sin Influx)'}")

    if not ok:
        contadores["influx_fallos"] += 1


# ---------------------------------------------------------------- MQTT
def al_conectar(cliente, _userdata, _flags, codigo, _props=None):
    if codigo == 0:
        cliente.subscribe(MQTT_TOPIC, qos=1)
        log(f"Conectado al broker {MQTT_HOST}:{MQTT_PUERTO} como '{MQTT_USUARIO}'. Escuchando {MQTT_TOPIC}")
    else:
        log(f"!! El broker rechazo la conexion: {codigo} (revisa usuario y clave)")


def al_desconectar(_cliente, _userdata, _flags, codigo, _props=None):
    log(f"Desconectado del broker ({codigo}). Reintento solo...")


def al_mensaje(_cliente, _userdata, msg):
    try:
        procesar(msg.topic, msg.payload)
    except Exception as e:  # un mensaje raro nunca debe tumbar el puente
        log(f"   !! Error procesando {msg.topic}: {e!r}")


def main():
    if not MQTT_CLAVE:
        log("Falta MQTT_CLAVE en .env")
        sys.exit(1)
    cliente = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2, client_id="puente-influx", clean_session=False)
    cliente.username_pw_set(MQTT_USUARIO, MQTT_CLAVE)
    cliente.on_connect = al_conectar
    cliente.on_disconnect = al_desconectar
    cliente.on_message = al_mensaje
    cliente.reconnect_delay_set(min_delay=1, max_delay=30)

    def salir(*_):
        log(f"Saliendo. Guardado: {contadores}")
        cliente.disconnect()
        sys.exit(0)

    signal.signal(signal.SIGTERM, salir)
    signal.signal(signal.SIGINT, salir)

    log(f"Puente MQTT -> InfluxDB ({INFLUX_URL}, bucket {INFLUX_BUCKET}) + CSV ({DATOS_DIR})")
    while True:
        try:
            cliente.connect(MQTT_HOST, MQTT_PUERTO, keepalive=30)
            break
        except OSError as e:
            log(f"No llego al broker ({e}). Reintento en 5 s.")
            time.sleep(5)
    cliente.loop_forever(retry_first_connection=True)


if __name__ == "__main__":
    main()
