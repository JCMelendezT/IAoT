#!/usr/bin/env python3
"""
Simulador MQTT del nodo: repite una grabacion real del detector y la publica en el
broker con los MISMOS topics que usa el devkit. Sirve para probar el broker, el puente,
InfluxDB y Grafana sin tener las placas a mano.

Uso (en Ubuntu, en tu PC):
    pip install --break-system-packages "paho-mqtt>=2,<3"   # una vez
    python3 simular_mqtt.py --host <IP publica de la VM> --usuario juan --clave TU_CLAVE
    python3 simular_mqtt.py ... --rapido          # sin esperar entre ventanas
    python3 simular_mqtt.py ... --dron            # sube la banda del dron para disparar la alerta

Topics que publica (nodo por defecto "simulador"):
    detector1/<nodo>/ventana       JSON con la ventana completa
    detector1/<nodo>/nivel_banda   numero (nivel en 2423-2456 MHz)
    detector1/<nodo>/aparatos      numero (aparatos WiFi vistos)
    detector1/<nodo>/latido        JSON cada 5 s
    detector1/<nodo>/evento        JSON cuando empieza o termina una alerta (solo con --dron)
"""
import argparse
import json
import time
from pathlib import Path

import paho.mqtt.client as mqtt

AQUI = Path(__file__).resolve().parent
POR_DEFECTO = AQUI.parent.parent / "practica-1" / "servidor" / "ejemplos" / "fondo_sin_dron_1min.txt"
CAMPOS_W = ["ms", "ventana", "canal", "tramas", "aparatos", "mgmt", "datos", "tam_prom",
            "rssi_prom", "rssi_max", "tramas_debiles", "aparatos_fuertes", "tramas_dji", "rssi_max_dji"]
BANDA = (23, 57)


def leer_lineas(archivo):
    for linea in Path(archivo).read_text(encoding="utf-8", errors="replace").splitlines():
        if linea.startswith("#") or " " not in linea:
            continue
        seg, resto = linea.split(" ", 1)
        try:
            yield float(seg), resto.strip()
        except ValueError:
            continue


def nivel(espectro):
    t = espectro[BANDA[0]:BANDA[1]]
    return round(sum(int(c) for c in t) / len(t), 2)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--host", required=True, help="IP publica de la VM con Mosquitto")
    ap.add_argument("--puerto", type=int, default=1883)
    ap.add_argument("--usuario", default="juan")
    ap.add_argument("--clave", required=True)
    ap.add_argument("--nodo", default="simulador")
    ap.add_argument("--archivo", default=str(POR_DEFECTO))
    ap.add_argument("--rapido", action="store_true")
    ap.add_argument("--dron", action="store_true", help="simula un dron en la mitad de la grabacion")
    ap.add_argument("--vueltas", type=int, default=1, help="cuantas veces repetir la grabacion (0 = sin fin)")
    a = ap.parse_args()

    base = f"detector1/{a.nodo}"
    c = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2, client_id=f"sim-{a.nodo}")
    c.username_pw_set(a.usuario, a.clave)
    c.will_set(f"{base}/estado", "desconectado", qos=1, retain=True)
    c.connect(a.host, a.puerto, keepalive=30)
    c.loop_start()
    time.sleep(1)
    if not c.is_connected():
        raise SystemExit("No pude conectarme al broker: revisa host, usuario y clave.")
    c.publish(f"{base}/estado", "conectado", qos=1, retain=True)
    print(f"Publicando {Path(a.archivo).name} en {a.host}:{a.puerto} como {a.usuario} -> {base}/...")

    lineas = list(leer_lineas(a.archivo))
    total = len([l for _, l in lineas if l.startswith("W,")])
    vuelta, enviadas, latidos = 0, 0, 0
    try:
        while a.vueltas == 0 or vuelta < a.vueltas:
            vuelta += 1
            filas, inicio, previo, n, en_alerta = [], time.time(), -5.0, 0, False
            for seg, linea in lineas:
                if not a.rapido:
                    espera = seg - (time.time() - inicio)
                    if espera > 0:
                        time.sleep(espera)
                if linea.startswith("N,"):
                    p = linea.split(",")
                    if len(p) == 3 and len(p[2]) == 126:
                        filas.append(p[2])
                elif linea.startswith("W,"):
                    vals = linea.split(",")[1:]
                    if len(vals) != len(CAMPOS_W):
                        continue
                    v = {k: (float(x) if k in ("tam_prom", "rssi_prom") else int(x)) for k, x in zip(CAMPOS_W, vals)}
                    esp = "".join(max(col) for col in zip(*filas)) if filas else None
                    filas = []
                    n += 1
                    dron = a.dron and total // 3 <= n < 2 * total // 3
                    if esp and dron:  # pinta energia fuerte en la banda del dron
                        esp = esp[:BANDA[0]] + "6" * (BANDA[1] - BANDA[0]) + esp[BANDA[1]:]
                    if esp:
                        v["espectro"] = esp
                        c.publish(f"{base}/nivel_banda", str(nivel(esp)), qos=0)
                    c.publish(f"{base}/aparatos", str(v["aparatos"]), qos=0)
                    c.publish(f"{base}/ventana", json.dumps(v), qos=1)
                    enviadas += 1
                    print(f"ventana #{v['ventana']}  banda {nivel(esp) if esp else '-'}{'  <<< dron simulado' if dron else ''}")
                    if dron != en_alerta and a.dron:
                        en_alerta = dron
                        tipo = "alerta_local" if dron else "fin_alerta"
                        c.publish(f"{base}/evento", json.dumps({"ms": v["ms"], "tipo": tipo, "detalle": "simulado"}), qos=1)
                        print(f">>> evento {tipo}")
                    if seg - previo >= 5:
                        latidos += 1
                        previo = seg
                        c.publish(f"{base}/latido", json.dumps({"contador": latidos, "ms": v["ms"], "rssi": -60}), qos=1)
    except KeyboardInterrupt:
        pass
    finally:
        c.publish(f"{base}/estado", "desconectado", qos=1, retain=True).wait_for_publish(3)
        c.loop_stop()
        c.disconnect()
    print(f"Listo: {enviadas} ventanas y {latidos} latidos publicados.")


if __name__ == "__main__":
    main()
