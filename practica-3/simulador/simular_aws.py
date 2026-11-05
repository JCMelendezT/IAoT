#!/usr/bin/env python3
"""Simula la placa para probar AWS IoT Core sin hardware (Practica 3).

Publica una lectura cada 5 s en detector1/nodo-s3/lectura (igual que devkit_aws.ino) y
escucha detector1/actuador/cmd: cuando el dashboard manda un comando, responde en
detector1/actuador/estado, asi se prueba todo el camino de ida y vuelta.

Uso (desde practica-3/simulador, despues de "terraform apply"):
    pip install --break-system-packages "paho-mqtt>=2,<3"
    python3 simular_aws.py              # trafico normal
    python3 simular_aws.py --dron       # sube la banda del dron: debe saltar la alerta
    python3 simular_aws.py --intervalo 1 --lecturas 30
"""
import argparse
import json
import os
import random
import ssl
import time

import paho.mqtt.client as mqtt

RAIZ = "detector1"
CERTS = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "certs")


def leer_conexion():
    datos = {}
    with open(os.path.join(CERTS, "conexion.env"), encoding="utf-8") as f:
        for linea in f:
            if "=" in linea:
                k, v = linea.strip().split("=", 1)
                datos[k] = v
    return datos


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--dron", action="store_true", help="simula energia alta en la banda del dron")
    ap.add_argument("--intervalo", type=float, default=5.0, help="segundos entre lecturas")
    ap.add_argument("--lecturas", type=int, default=0, help="0 = hasta Ctrl+C")
    args = ap.parse_args()

    conf = leer_conexion()
    estado = {"on": False}
    t_cmd, t_estado = f"{RAIZ}/actuador/cmd", f"{RAIZ}/actuador/estado"
    t_lectura = f"{RAIZ}/nodo-s3/lectura"

    cli = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2, client_id=conf["COSA"] + "-sim")
    cli.tls_set(ca_certs=os.path.join(CERTS, "AmazonRootCA1.pem"),
                certfile=os.path.join(CERTS, "certificado.pem"),
                keyfile=os.path.join(CERTS, "llave-privada.pem"),
                tls_version=ssl.PROTOCOL_TLS_CLIENT)

    def al_conectar(c, _u, _f, rc, _p):
        print("Conectado a AWS IoT Core" if rc == 0 else f"Fallo al conectar: {rc}")
        c.subscribe(t_cmd, qos=1)
        c.publish(t_estado, json.dumps({"estado": "off"}))

    def al_mensaje(c, _u, msg):
        print(f"<<< {msg.topic}: {msg.payload.decode()}")
        try:
            orden = json.loads(msg.payload).get("estado")
        except ValueError:
            return
        if orden in ("on", "off"):
            estado["on"] = orden == "on"
            c.publish(t_estado, json.dumps({"estado": orden}))
            print(f">>> ACTUADOR {'ENCENDIDO' if estado['on'] else 'APAGADO'}")

    cli.on_connect, cli.on_message = al_conectar, al_mensaje
    cli.connect(conf["IOT_ENDPOINT"], 8883, keepalive=30)
    cli.loop_start()

    n = 0
    try:
        while args.lecturas == 0 or n < args.lecturas:
            n += 1
            nivel = random.uniform(0.2, 1.2)
            if args.dron and (n // 6) % 2 == 1:          # 6 lecturas normales, 6 con "dron"
                nivel = random.uniform(3.0, 5.5)
            lectura = {
                "nivel_banda": round(nivel, 2),
                "aparatos": random.randint(3, 12),
                "tramas": random.randint(80, 400),
                "rssi_max": -random.randint(40, 75),
                "ventana": n,
                "canal": 6,
                "alerta": 1 if nivel > 2.5 else 0,
                "wifi_rssi": -random.randint(45, 70),
            }
            cli.publish(t_lectura, json.dumps(lectura), qos=1)
            print(f"Lectura #{n}: banda {lectura['nivel_banda']}, {lectura['aparatos']} aparatos"
                  + ("  [ALERTA]" if lectura["alerta"] else ""))
            time.sleep(args.intervalo)
    except KeyboardInterrupt:
        pass
    cli.loop_stop()
    cli.disconnect()


if __name__ == "__main__":
    main()
