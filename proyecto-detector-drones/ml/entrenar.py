"""
Paso 2: entrena el clasificador fondo / bluetooth / dron y lo exporta para el ESP32-S3.

Como cada clase se grabo en una sola toma larga (~10 min), no hay sesiones separadas.
Para no hacer trampa al evaluar, las ventanas se agrupan en bloques de 1 minuto y nunca
se usa el mismo minuto para entrenar y para probar (ventanas vecinas se parecen mucho).
Ademas se hace una prueba final: entrenar con el primer 70 % de cada grabacion y probar
con el ultimo 30 %, dejando 10 s de separacion.

Uso:  python entrenar.py        (antes: python rasgos.py)
Sale: ../modelos/modelo_detector.h   -> para el firmware (se copia tambien a
                                        Lab_ESP32/firmware/nodo_detector/)
      ../modelos/ficha_modelo.md     -> metricas
      ../resultados/matriz_confusion.png
"""

import json
import os
import shutil
from datetime import date

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np
import pandas as pd
from sklearn.linear_model import LogisticRegression
from sklearn.metrics import accuracy_score, classification_report, confusion_matrix, f1_score
from sklearn.model_selection import GroupKFold, cross_val_predict
from sklearn.pipeline import make_pipeline
from sklearn.preprocessing import StandardScaler

import rasgos as R

AQUI = os.path.dirname(os.path.abspath(__file__))
ENTRADA = os.path.join(AQUI, "..", "captura-datos", "procesado", "ventanas.csv")
CARPETA_MODELOS = os.path.join(AQUI, "..", "modelos")
CARPETA_RESULTADOS = os.path.join(AQUI, "..", "resultados")
CARPETA_FIRMWARE = os.path.join(AQUI, "..", "..", "Lab_ESP32", "firmware", "nodo_detector")
CLASES = ["fondo", "bluetooth", "dron"]
# Con los rasgos del trafico WiFi el modelo aprendio "poco trafico = dron" y de noche, con la
# casa quieta, daba falsas alarmas. Desde el 2026-11-12 se entrena solo con el espectro del nRF24.
USAR_RASGOS_WIFI = False
BLOQUES = 10                 # pliegues de la validacion por bloques de 1 minuto
FRACCION_ENTRENO = 0.7
SEPARACION_S = 10


def nuevo_modelo():
    return make_pipeline(StandardScaler(),
                         LogisticRegression(max_iter=5000, class_weight="balanced"))


def falsas_alarmas(y, pred, t, sesiones):
    """Ventanas sin dron marcadas como dron, sola y con la regla de 2 ventanas seguidas."""
    sueltas = seguidas = 0
    for s in np.unique(sesiones):
        m = sesiones == s
        orden = np.argsort(t[m])
        yy, pp = y[m][orden], pred[m][orden]
        es_dron = pp == "dron"
        sueltas += int(((yy != "dron") & es_dron).sum())
        dos = es_dron[1:] & es_dron[:-1]
        seguidas += int(((yy[1:] != "dron") & dos).sum())
    return sueltas, seguidas


def exportar_c(modelo, rasgos, ruta, resumen):
    escala = modelo.named_steps["standardscaler"]
    logis = modelo.named_steps["logisticregression"]
    clases = list(logis.classes_)
    orden = [clases.index(c) for c in CLASES]          # siempre fondo, bluetooth, dron
    coef = logis.coef_[orden]
    sesgo = logis.intercept_[orden]

    def lista(v):
        return ", ".join("{:.7g}f".format(x) for x in v)

    lineas = [
        "// Modelo del detector de drones (regresion logistica, 3 clases).",
        "// Generado por proyecto-detector-drones/ml/entrenar.py el {}. No editar a mano.".format(date.today()),
        "// {}".format(resumen),
        "//",
        "// Uso: llena x[] con los rasgos de una ventana de 2 s EN ESTE ORDEN y llama",
        "//      int clase = detectorClasificar(x, prob);   // 0=fondo 1=bluetooth 2=dron",
        "#pragma once",
        "#include <math.h>",
        "",
        "#define DETECTOR_N_RASGOS {}".format(len(rasgos)),
        "#define DETECTOR_N_CLASES 3",
        "",
        "static const char* const DETECTOR_CLASES[DETECTOR_N_CLASES] = {{{}}};".format(
            ", ".join('"{}"'.format(c) for c in CLASES)),
        "",
        "// Orden de los rasgos:",
    ]
    lineas += ["//   {:2d} {}".format(i, r) for i, r in enumerate(rasgos)]
    lineas += [
        "static const char* const DETECTOR_RASGOS[DETECTOR_N_RASGOS] = {",
        "  " + ", ".join('"{}"'.format(r) for r in rasgos),
        "};",
        "",
        "static const float DETECTOR_MEDIA[DETECTOR_N_RASGOS] = {{{}}};".format(lista(escala.mean_)),
        "static const float DETECTOR_ESCALA[DETECTOR_N_RASGOS] = {{{}}};".format(lista(escala.scale_)),
        "static const float DETECTOR_PESOS[DETECTOR_N_CLASES][DETECTOR_N_RASGOS] = {",
    ]
    lineas += ["  {{{}}},  // {}".format(lista(fila), c) for fila, c in zip(coef, CLASES)]
    lineas += [
        "};",
        "static const float DETECTOR_SESGO[DETECTOR_N_CLASES] = {{{}}};".format(lista(sesgo)),
        "",
        "// Devuelve la clase mas probable; si prob no es NULL, deja ahi las 3 probabilidades.",
        "static inline int detectorClasificar(const float* x, float* prob) {",
        "  float z[DETECTOR_N_CLASES];",
        "  float maximo = -1e30f;",
        "  for (int c = 0; c < DETECTOR_N_CLASES; c++) {",
        "    float s = DETECTOR_SESGO[c];",
        "    for (int i = 0; i < DETECTOR_N_RASGOS; i++)",
        "      s += DETECTOR_PESOS[c][i] * (x[i] - DETECTOR_MEDIA[i]) / DETECTOR_ESCALA[i];",
        "    z[c] = s;",
        "    if (s > maximo) maximo = s;",
        "  }",
        "  float suma = 0, mejor = -1; int clase = 0;",
        "  for (int c = 0; c < DETECTOR_N_CLASES; c++) {",
        "    z[c] = expf(z[c] - maximo);",
        "    suma += z[c];",
        "  }",
        "  for (int c = 0; c < DETECTOR_N_CLASES; c++) {",
        "    float p = z[c] / suma;",
        "    if (prob) prob[c] = p;",
        "    if (p > mejor) { mejor = p; clase = c; }",
        "  }",
        "  return clase;",
        "}",
        "",
    ]
    with open(ruta, "w", encoding="utf-8") as f:
        f.write("\n".join(lineas))
    return clases


def dibujar_matriz(cm, titulo, ruta):
    fig, ax = plt.subplots(figsize=(4.6, 4))
    ax.imshow(cm, cmap="Blues")
    ax.set_xticks(range(3), CLASES)
    ax.set_yticks(range(3), CLASES)
    ax.set_xlabel("Lo que dijo el modelo")
    ax.set_ylabel("Lo que era en realidad")
    ax.set_title(titulo, fontsize=10)
    for i in range(3):
        for j in range(3):
            ax.text(j, i, cm[i, j], ha="center", va="center",
                    color="white" if cm[i, j] > cm.max() / 2 else "black")
    fig.tight_layout()
    fig.savefig(ruta, dpi=150)
    plt.close(fig)


def main():
    d = pd.read_csv(ENTRADA)
    d = d[d.clase.isin(CLASES)].reset_index(drop=True)
    rasgos = [r for r in R.RASGOS if d[r].std() > 0]   # fuera los que siempre valen lo mismo
    if not USAR_RASGOS_WIFI:
        rasgos = [r for r in rasgos if r not in R.RASGOS_WIFI]
    X, y = d[rasgos].values, d.clase.values
    t, ses = d.t.values, d.sesion.values
    grupos = (d.sesion + "_" + d.minuto.astype(str)).values

    # 1) Validacion por bloques de 1 minuto
    pred_cv = cross_val_predict(nuevo_modelo(), X, y, groups=grupos, cv=GroupKFold(BLOQUES))
    cm_cv = confusion_matrix(y, pred_cv, labels=CLASES)
    fa_cv = falsas_alarmas(y, pred_cv, t, ses)

    # 2) Prueba final: primer 70 % entrena, ultimo 30 % prueba
    entreno, prueba = [], []
    for s, dd in d.groupby("sesion"):
        corte = dd.t.max() * FRACCION_ENTRENO
        entreno += list(dd.index[dd.t < corte - SEPARACION_S])
        prueba += list(dd.index[dd.t >= corte])
    m = nuevo_modelo().fit(X[entreno], y[entreno])
    pred_ho = m.predict(X[prueba])
    cm_ho = confusion_matrix(y[prueba], pred_ho, labels=CLASES)
    fa_ho = falsas_alarmas(y[prueba], pred_ho, t[prueba], ses[prueba])

    # 3) Modelo final con todos los datos
    final = nuevo_modelo().fit(X, y)
    os.makedirs(CARPETA_MODELOS, exist_ok=True)
    os.makedirs(CARPETA_RESULTADOS, exist_ok=True)
    acc_cv, acc_ho = accuracy_score(y, pred_cv), accuracy_score(y[prueba], pred_ho)
    resumen = "Exactitud: {:.1%} (bloques de 1 min) y {:.1%} (ultimo 30 % de cada grabacion).".format(acc_cv, acc_ho)
    ruta_h = os.path.join(CARPETA_MODELOS, "modelo_detector.h")
    exportar_c(final, rasgos, ruta_h, resumen)
    if os.path.isdir(CARPETA_FIRMWARE):     # el sketch necesita su propia copia
        shutil.copyfile(ruta_h, os.path.join(CARPETA_FIRMWARE, "modelo_detector.h"))
    dibujar_matriz(cm_cv, "Validacion por bloques de 1 min\n(exactitud {:.1%})".format(acc_cv),
                   os.path.join(CARPETA_RESULTADOS, "matriz_confusion.png"))

    # Para comprobar que el firmware calcula igual que Python
    np.savetxt(os.path.join(CARPETA_MODELOS, "prueba_rasgos.csv"), X[:20], delimiter=",", fmt="%.6g")
    np.savetxt(os.path.join(CARPETA_MODELOS, "prueba_prob.csv"),
               final.predict_proba(X[:20])[:, [list(final.classes_).index(c) for c in CLASES]],
               delimiter=",", fmt="%.6f")

    horas_sin_dron = (y != "dron").sum() * R.VENTANA_S / 3600
    horas_sin_dron_ho = (y[prueba] != "dron").sum() * R.VENTANA_S / 3600
    rep = classification_report(y, pred_cv, labels=CLASES, digits=3)
    conteo = d.groupby(["clase_grabada", "estado"]).size()

    ficha = [
        "# Ficha del modelo: detector de drones",
        "",
        "Generada por `ml/entrenar.py` el {}.".format(date.today()),
        "",
        "- **Modelo:** regresion logistica con normalizacion, 3 clases (fondo, bluetooth, dron).",
        "- **Rasgos:** {} por ventana de 2 s ({} del WiFi y {} del espectro nRF24).".format(
            len(rasgos), sum(r in R.RASGOS_WIFI for r in rasgos), sum(r not in R.RASGOS_WIFI for r in rasgos)),
        "- **Tamano:** {} numeros (~{} bytes en float32).".format(
            len(rasgos) * 5 + 3, (len(rasgos) * 5 + 3) * 4),
        "- **Datos:** {} ventanas, una grabacion larga por clase, en el estudio, a 1 m, el {}.".format(
            len(d), d.sesion.str[:8].iloc[0]),
        "",
        "| Grabacion | Estado | Ventanas |",
        "| --- | --- | --- |",
    ]
    ficha += ["| {} | {} | {} |".format(c, e, n) for (c, e), n in conteo.items()]
    ficha += [
        "",
        "## Resultados",
        "",
        "| Prueba | Exactitud | F1 promedio | Falsas alarmas de dron (sueltas) | Con regla de 2 ventanas seguidas |",
        "| --- | --- | --- | --- | --- |",
        "| Bloques de 1 min (10 pliegues) | {:.1%} | {:.3f} | {} en {:.2f} h sin dron | {} |".format(
            acc_cv, f1_score(y, pred_cv, average="macro"), fa_cv[0], horas_sin_dron, fa_cv[1]),
        "| Ultimo 30 % de cada grabacion | {:.1%} | {:.3f} | {} en {:.2f} h sin dron | {} |".format(
            acc_ho, f1_score(y[prueba], pred_ho, average="macro"), fa_ho[0], horas_sin_dron_ho, fa_ho[1]),
        "",
        "Matriz de confusion (bloques de 1 min; filas = realidad, columnas = modelo):",
        "",
        "| | fondo | bluetooth | dron |",
        "| --- | --- | --- | --- |",
    ]
    ficha += ["| **{}** | {} |".format(c, " | ".join(str(v) for v in fila)) for c, fila in zip(CLASES, cm_cv)]
    ficha += [
        "",
        "```",
        rep.rstrip(),
        "```",
        "",
        "## Limites (van en el informe)",
        "",
        "- Todo se grabo el mismo dia, en el mismo sitio y a 1 m. No se sabe aun como responde a 3, 5 o 10 m, ni en otro lugar.",
        "- Cada clase es una sola grabacion seguida: el modelo pudo aprender algo propio de esa hora.",
        "  Paso con el trafico WiFi: con esos rasgos el modelo decia \"poco trafico = dron\" y de noche daba falsas alarmas.",
        "  Por eso este modelo usa solo el espectro del nRF24 (USAR_RASGOS_WIFI = False en entrenar.py).",
        "- Solo detecta el Neo 2 con su control en 2,4 GHz; en 5,8 GHz o solo con el celular no se ve.",
        "",
    ]
    with open(os.path.join(CARPETA_MODELOS, "ficha_modelo.md"), "w", encoding="utf-8") as f:
        f.write("\n".join(ficha))
    with open(os.path.join(CARPETA_MODELOS, "metricas.json"), "w", encoding="utf-8") as f:
        json.dump({"rasgos": rasgos, "exactitud_bloques": acc_cv, "exactitud_ultimo_30": acc_ho,
                   "falsas_alarmas_bloques": fa_cv, "falsas_alarmas_ultimo_30": fa_ho,
                   "matriz_bloques": cm_cv.tolist(), "matriz_ultimo_30": cm_ho.tolist()}, f, indent=2)

    print(resumen)
    print(pd.DataFrame(cm_cv, index=CLASES, columns=CLASES))
    print("Falsas alarmas (sueltas / 2 seguidas): bloques {} | ultimo 30 % {}".format(fa_cv, fa_ho))
    print("Listo: modelos/modelo_detector.h, modelos/ficha_modelo.md, resultados/matriz_confusion.png")


if __name__ == "__main__":
    main()
