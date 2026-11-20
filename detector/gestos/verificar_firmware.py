"""Comprueba que el codigo de la camara (C++) da las mismas probabilidades que NumPy.

Crea fotos y un modelo de prueba, compila firmware/camara_gestos/prueba_pc y compara.
    python verificar_firmware.py
"""
import os
import subprocess
import sys
import tempfile

import numpy as np

import entrenar_gestos as g

AQUI = os.path.dirname(os.path.abspath(__file__))
FIRMWARE = os.path.normpath(os.path.join(AQUI, "..", "..", "firmware", "camara_gestos"))


def escribir_pgm(ruta, img):
    with open(ruta, "wb") as f:
        f.write(b"P5\n%d %d\n255\n" % (g.ANCHO, g.ALTO) + img.tobytes())


def main():
    rng = np.random.default_rng(5)
    with tempfile.TemporaryDirectory() as tmp:
        imgs, y, grupos, orden = g.fotos_inventadas(rng, n_sesiones=1, por_sesion=8)
        rutas = []
        for i, im in enumerate(imgs):
            r = os.path.join(tmp, f"{g.CLASES[y[i]]}_s01_{i:04d}.pgm")
            escribir_pgm(r, im)
            rutas.append(r)

        p = g.iniciar(9)
        for k in p:                                   # sesgos distintos de cero para probar todo
            if k.endswith("b"):
                p[k] += rng.normal(0, 0.2, p[k].shape).astype(np.float32)
        media, desv = float(imgs.mean() / 255), float(imgs.std() / 255)
        modelo = os.path.join(tmp, "modelo.bin")
        g.guardar_modelo(modelo, p, media, desv)

        exe = os.path.join(tmp, "probar_cnn")
        subprocess.run(["g++", "-O2", "-I", FIRMWARE, os.path.join(FIRMWARE, "prueba_pc", "probar_cnn.cpp"),
                        "-o", exe], check=True)
        salida = subprocess.run([exe, modelo] + rutas, check=True, capture_output=True, text=True).stdout
        c = np.array([[float(v) for v in linea.split(",")[1:]] for linea in salida.strip().splitlines()])
        p2, m2, d2 = g.cargar_modelo(modelo)
        ref = g.predecir(p2, np.array([g.leer_pgm(r) for r in rutas]), m2, d2)
        dif = float(np.abs(c - ref).max())
        print(f"{len(rutas)} fotos, diferencia maxima C++ contra NumPy: {dif:.2e}", "OK" if dif < 1e-4 else "MAL")
        ok = dif < 1e-4

        # un archivo danado se tiene que rechazar
        datos = bytearray(open(modelo, "rb").read())
        datos[100] ^= 0xFF
        malo = os.path.join(tmp, "malo.bin")
        open(malo, "wb").write(datos)
        r = subprocess.run([exe, malo, rutas[0]], capture_output=True, text=True)
        rechazado = r.returncode == 1 and r.stdout.startswith("ERROR")
        print("modelo danado:", "rechazado OK" if rechazado else "NO lo rechazo, MAL")
        sys.exit(0 if ok and rechazado else 1)


if __name__ == "__main__":
    main()
