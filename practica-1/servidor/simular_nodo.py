#!/usr/bin/env python3
"""
Simulador del nodo: repite una grabacion real del capturador contra el servidor,
como si la placa la estuviera mandando en vivo. Sirve para probar el servidor y
la pagina sin tener las placas conectadas.

Uso (en Ubuntu, con el servidor encendido en otra ventana):
    python3 simular_nodo.py
    python3 simular_nodo.py --archivo ruta/a/otra_grabacion.txt
    python3 simular_nodo.py --rapido        # sin esperar los 2 s entre ventanas

Solo usa grabaciones hechas con nodo_captura (lineas N, y W,).
"""
import argparse
import json
import time
import urllib.error
import urllib.request
from pathlib import Path

AQUI = Path(__file__).resolve().parent
# Grabacion real incluida en el repositorio (1 minuto, sin dron, 2026-10-20).
POR_DEFECTO = AQUI / "ejemplos" / "fondo_sin_dron_1min.txt"

CAMPOS_W = ["ms", "ventana", "canal", "tramas", "aparatos", "mgmt", "datos", "tam_prom",
            "rssi_prom", "rssi_max", "tramas_debiles", "aparatos_fuertes", "tramas_dji", "rssi_max_dji"]


def enviar(servidor, ruta, datos):
    peticion = urllib.request.Request(
        servidor.rstrip("/") + ruta, data=json.dumps(datos).encode(),
        headers={"Content-Type": "application/json"}, method="POST")
    try:
        with urllib.request.urlopen(peticion, timeout=5) as r:
            return r.status, json.loads(r.read() or b"{}")
    except urllib.error.HTTPError as e:
        return e.code, e.read().decode(errors="replace")
    except urllib.error.URLError as e:
        return None, str(e.reason)


def leer_lineas(archivo):
    """Devuelve (segundo, linea) de la grabacion."""
    for linea in Path(archivo).read_text(encoding="utf-8", errors="replace").splitlines():
        if linea.startswith("#") or " " not in linea:
            continue
        seg, resto = linea.split(" ", 1)
        try:
            yield float(seg), resto.strip()
        except ValueError:
            continue


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--servidor", default="http://localhost:8000")
    ap.add_argument("--archivo", default=str(POR_DEFECTO))
    ap.add_argument("--nodo", default="simulador")
    ap.add_argument("--rapido", action="store_true")
    args = ap.parse_args()

    print(f"Repitiendo {Path(args.archivo).name} contra {args.servidor}")
    filas_espectro, inicio, enviadas, latidos, previo = [], time.time(), 0, 0, 0.0
    for seg, linea in leer_lineas(args.archivo):
        if not args.rapido:
            espera = seg - (time.time() - inicio)
            if espera > 0:
                time.sleep(espera)

        if linea.startswith("N,"):
            partes = linea.split(",")
            if len(partes) == 3 and len(partes[2]) == 126:
                filas_espectro.append(partes[2])

        elif linea.startswith("W,"):
            valores = linea.split(",")[1:]
            if len(valores) != len(CAMPOS_W):
                continue
            ventana = {k: (float(v) if k in ("tam_prom", "rssi_prom") else int(v)) for k, v in zip(CAMPOS_W, valores)}
            ventana["nodo"] = args.nodo
            if filas_espectro:
                # Resumen del espectro de la ventana: el maximo de cada canal.
                ventana["espectro"] = "".join(max(col) for col in zip(*filas_espectro))
            filas_espectro = []
            codigo, resp = enviar(args.servidor, "/api/ventanas", ventana)
            enviadas += 1
            marca = "  <<< ALERTA" if isinstance(resp, dict) and resp.get("alerta") else ""
            print(f"ventana #{ventana['ventana']} -> {codigo}{marca}" if codigo == 200
                  else f"ventana #{ventana['ventana']} -> FALLO {codigo}: {resp}")

            if seg - previo >= 5 or latidos == 0:
                latidos += 1
                previo = seg
                enviar(args.servidor, "/api/latido",
                       {"nodo": args.nodo, "contador": latidos, "ms": ventana["ms"], "rssi": -60})

    print(f"Listo: {enviadas} ventanas y {latidos} latidos enviados.")


if __name__ == "__main__":
    main()
