"""
Compara lo que calcula el firmware (compilado en el PC) con ml/rasgos.py y el modelo entrenado.

    g++ -O2 -I.. prueba_rasgos.cpp -o prueba_rasgos
    ./prueba_rasgos ../../../../proyecto-detector-drones/captura-datos/raw/20261110_*.txt > c_ventanas.csv
    python comparar.py c_ventanas.csv
"""
import os
import sys

import numpy as np
import pandas as pd

ML = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "..", "..", "proyecto-detector-drones", "ml")
sys.path.insert(0, ML)
import entrenar as E  # noqa: E402
import rasgos as R    # noqa: E402

d = pd.read_csv(E.ENTRADA)
rasgos = [r for r in R.RASGOS if d[r].std() > 0]
if not E.USAR_RASGOS_WIFI:
    rasgos = [r for r in rasgos if r not in R.RASGOS_WIFI]
c = pd.read_csv(sys.argv[1], header=None, names=["sesion", "t"] + rasgos + ["p0", "p1", "p2", "clase_c"])
m = d.merge(c, on=["sesion", "t"], suffixes=("", "_c"))
modelo = E.nuevo_modelo().fit(d[rasgos].values, d.clase.values)
orden = [list(modelo.classes_).index(k) for k in E.CLASES]
prob = modelo.predict_proba(m[rasgos].values)[:, orden]
print("Ventanas: Python {}, firmware {}, emparejadas {}".format(len(d), len(c), len(m)))
print("Mayor diferencia en un rasgo: {:.2g} (Python guarda 4 decimales)".format(
    max(np.abs(m[r] - m[r + "_c"]).max() for r in rasgos)))
print("Mayor diferencia en una probabilidad: {:.2g}".format(np.abs(prob - m[["p0", "p1", "p2"]].values).max()))
print("Ventanas con clase distinta: {}".format(int((modelo.predict(m[rasgos].values) != m.clase_c).sum())))
