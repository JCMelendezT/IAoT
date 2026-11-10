"""
Paso 1: convierte las grabaciones crudas (captura-datos/raw/*.txt) en una tabla de rasgos.
Solo usa las sesiones de sesiones.csv grabadas con nodo_captura, y omite las marcadas
como exploratorias o de prueba en las notas.

Una fila = una ventana de 2 s (cada linea W del ESP32), con:
  - los 9 numeros del resumen WiFi de esa ventana
  - el espectro del nRF24 de esos mismos 2 s, resumido en 21 bandas de 6 MHz y unas pocas medidas

Uso:  python rasgos.py
Sale: procesado/ventanas.csv
"""

import csv
import glob
import os

import numpy as np

AQUI = os.path.dirname(os.path.abspath(__file__))
CARPETA_RAW = os.path.join(AQUI, "..", "captura-datos", "raw")
SESIONES = os.path.join(AQUI, "..", "captura-datos", "sesiones.csv")
SALIDA = os.path.join(AQUI, "..", "captura-datos", "procesado", "ventanas.csv")
# Sesiones que no entran al entrenamiento si sus notas dicen esto
DESCARTAR_SI_NOTA = ["exploratoria", "prueba", "no usar"]

CANALES = 126                 # nRF24: canal c = 2400 + c MHz
ANCHO_BANDA = 6               # 126 / 6 = 21 bandas
BANDA_DRON = (23, 57)         # 2423-2456 MHz, donde aparecio el Neo 2
VENTANA_S = 2.0

CAMPOS_W = ["ms", "ventana", "canal", "tramas", "aparatos", "mgmt", "datos", "tam_prom",
            "rssi_prom", "rssi_max", "tramas_debiles", "aparatos_fuertes",
            "tramas_dji", "rssi_max_dji"]
# Del resumen WiFi se usan estos (los de DJI siempre dan 0 con el control en 2,4 GHz)
RASGOS_WIFI = ["tramas", "aparatos", "mgmt", "datos", "tam_prom",
               "rssi_prom", "rssi_max", "tramas_debiles", "aparatos_fuertes"]
RASGOS_ESPECTRO = (["banda_{:03d}".format(2400 + i * ANCHO_BANDA) for i in range(CANALES // ANCHO_BANDA)]
                   + ["esp_media", "esp_banda_dron", "esp_max_banda_dron",
                      "esp_canales_activos", "esp_variacion"])
RASGOS = RASGOS_WIFI + RASGOS_ESPECTRO

# Las dos tomas del dron (tierra y vuelo) son la misma clase para el modelo
CLASE_MODELO = {"fondo": "fondo", "bluetooth": "bluetooth", "celular-video": "celular",
                "dron-control": "dron"}


def leer_grabacion(ruta):
    """Devuelve (cabecera, filas_N, filas_W). filas_N = [(t, vector126)], filas_W = [(t, dict)]."""
    cab, filas_n, filas_w = {}, [], []
    with open(ruta, encoding="utf-8") as f:
        for linea in f:
            linea = linea.strip()
            if linea.startswith("#"):
                if ":" in linea:
                    k, v = linea[1:].split(":", 1)
                    cab[k.strip()] = v.strip()
                continue
            try:
                t_txt, dato = linea.split(" ", 1)
                t = float(t_txt)
            except ValueError:
                continue
            if dato.startswith("N,"):
                partes = dato.split(",", 2)
                if len(partes) == 3 and len(partes[2]) == CANALES and partes[2].isdigit():
                    filas_n.append((t, np.array([int(c) for c in partes[2]], dtype=float)))
            elif dato.startswith("W,"):
                partes = dato.split(",")[1:]
                if len(partes) == len(CAMPOS_W):
                    filas_w.append((t, dict(zip(CAMPOS_W, (int(x) for x in partes)))))
    return cab, filas_n, filas_w


def rasgos_espectro(matriz):
    """matriz: filas del nRF24 dentro de la ventana (k x 126), valores 0..8."""
    media = matriz.mean(axis=0)
    bandas = media.reshape(-1, ANCHO_BANDA).mean(axis=1)
    a, b = BANDA_DRON
    return list(bandas) + [
        media.mean(),
        media[a:b].mean(),
        matriz[:, a:b].max(),
        (media > 0.5).mean(),
        matriz.mean(axis=1).std(),     # cuanto cambia la actividad de una fila a otra
    ]


def procesar(ruta):
    cab, filas_n, filas_w = leer_grabacion(ruta)
    tiempos_n = np.array([t for t, _ in filas_n])
    espectro = np.array([v for _, v in filas_n])
    salida = []
    for t, w in filas_w:
        dentro = (tiempos_n > t - VENTANA_S) & (tiempos_n <= t)
        if dentro.sum() < 3:          # ventana sin espectro suficiente (al inicio)
            continue
        fila = {
            "sesion": cab.get("sesion", os.path.basename(ruta)),
            "clase_grabada": cab.get("clase", ""),
            "estado": cab.get("estado", ""),
            "clase": CLASE_MODELO.get(cab.get("clase", ""), cab.get("clase", "")),
            "t": round(t, 3),
            "minuto": int(t // 60),
        }
        fila.update({k: w[k] for k in RASGOS_WIFI})
        fila.update(dict(zip(RASGOS_ESPECTRO, (round(float(x), 4) for x in rasgos_espectro(espectro[dentro])))))
        salida.append(fila)
    return salida


def sesiones_validas():
    """Grabaciones de sesiones.csv hechas con nodo_captura que no sean pruebas ni exploratorias."""
    if not os.path.exists(SESIONES):          # sin registro: usa todo lo que haya en raw/
        return sorted(glob.glob(os.path.join(CARPETA_RAW, "*.txt")))
    archivos = []
    with open(SESIONES, encoding="utf-8") as f:
        for s in csv.DictReader(f):
            notas = (s.get("notas") or "").lower()
            ruta = os.path.join(CARPETA_RAW, os.path.basename(s["archivo_raw"]))
            if s.get("sensor") != "nodo-captura" or any(p in notas for p in DESCARTAR_SI_NOTA):
                print("  (se omite {}: {})".format(s["sesion_id"], s.get("notas") or s.get("sensor")))
            elif not os.path.exists(ruta):
                print("  (falta el archivo de {})".format(s["sesion_id"]))
            else:
                archivos.append(ruta)
    return archivos


def main():
    todas = []
    for ruta in sesiones_validas():
        filas = procesar(ruta)
        print("{:55s} {:4d} ventanas".format(os.path.basename(ruta), len(filas)))
        todas += filas
    os.makedirs(os.path.dirname(SALIDA), exist_ok=True)
    with open(SALIDA, "w", newline="", encoding="utf-8") as f:
        w = csv.DictWriter(f, fieldnames=list(todas[0].keys()))
        w.writeheader()
        w.writerows(todas)
    print("Total: {} ventanas -> {}".format(len(todas), os.path.relpath(SALIDA, AQUI)))


if __name__ == "__main__":
    main()
