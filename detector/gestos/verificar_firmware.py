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
        ok = ok and rechazado

        # modelo version 2 (4 capas, BatchNorm fundida) hecho con PyTorch
        import torch
        import entrenar_gestos_torch as t
        torch.manual_seed(3)
        m = t.Red(t.CANALES, media, desv)
        for bn in m.bns:
            bn.running_mean.normal_(0, 0.3); bn.running_var.uniform_(0.5, 2.0)
            bn.weight.data.uniform_(0.5, 1.5); bn.bias.data.normal_(0, 0.3)
        m.eval()
        modelo2 = os.path.join(tmp, "modelo2.bin")
        t.guardar_modelo(modelo2, m, media, desv)
        salida = subprocess.run([exe, modelo2] + rutas, check=True, capture_output=True, text=True).stdout
        c2 = np.array([[float(v) for v in linea.split(",")[1:]] for linea in salida.strip().splitlines()])
        ref2 = t.probabilidades(m, np.array([g.leer_pgm(r) for r in rutas]))
        dif2 = float(np.abs(c2 - ref2).max())
        print(f"modelo v2: diferencia maxima C++ contra PyTorch: {dif2:.2e}", "OK" if dif2 < 1e-4 else "MAL")
        datos = bytearray(open(modelo2, "rb").read())
        datos[200] ^= 0xFF
        open(malo, "wb").write(datos)
        r = subprocess.run([exe, malo, rutas[0]], capture_output=True, text=True)
        rech2 = r.returncode == 1 and r.stdout.startswith("ERROR")
        print("modelo v2 danado:", "rechazado OK" if rech2 else "NO lo rechazo, MAL")
        sys.exit(0 if ok and dif2 < 1e-4 and rech2 else 1)


if __name__ == "__main__":
    main()
