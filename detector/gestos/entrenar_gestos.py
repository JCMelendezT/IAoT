"""Entrena el reconocedor de gestos de la camara: nada, abierta y puno.

Es una red convolucional pequena escrita solo con NumPy (4803 numeros, 19 KB), asi se instala
en la ESP32-CAM sin librerias de IA y el codigo de la placa hace exactamente el mismo calculo.

Entrada: fotos de 80x60 en gris que baja la pagina de la camara. Cada rafaga llega como un
.zip (puno_s03.zip) con fotos llamadas
    <clase>_s<sesion>_<numero>.pgm      por ejemplo   puno_s03_0012.pgm
Se leen los .zip y los .pgm sueltos que haya en la carpeta fotos/.
Cada rafaga es una sesion. La validacion se hace con sesiones completas que el modelo no vio;
si se mezclaran fotos de la misma rafaga, el resultado saldria mucho mas bonito de lo real.

Uso:
    python entrenar_gestos.py                 # lee fotos/ y escribe modelo/modelo_gestos.bin
    python entrenar_gestos.py --autoprueba    # comprueba el codigo con datos inventados
"""
import argparse
import glob
import os
import re
import struct
import sys
import time
import zipfile

import numpy as np

AQUI = os.path.dirname(os.path.abspath(__file__))
CLASES = ["nada", "abierta", "puno"]
ANCHO, ALTO = 80, 60
CANALES = (8, 16, 24)
VERSION = 1
CAMARA_FPS = 4.0            # la camara mira unas 4 veces por segundo
VENTANA, MINIMO, CONFIANZA = 6, 5, 0.80   # un gesto vale si sale 5 de 6 veces con 80 %


# ----------------------------------------------------------------------------- datos
def leer_pgm(ruta, datos=None):
    if datos is None:
        with open(ruta, "rb") as f:
            datos = f.read()
    m = re.match(rb"P5\s+(\d+)\s+(\d+)\s+(\d+)\s", datos)
    if not m:
        raise ValueError(f"{ruta}: no es un PGM binario")
    w, h = int(m.group(1)), int(m.group(2))
    if (w, h) != (ANCHO, ALTO):
        raise ValueError(f"{ruta}: mide {w}x{h} y debe medir {ANCHO}x{ALTO}")
    img = np.frombuffer(datos, dtype=np.uint8, offset=m.end(), count=w * h)
    return img.reshape(h, w)


def cargar_fotos(carpeta):
    """Lee los .pgm sueltos y los .zip que baja la pagina de la camara (uno por rafaga)."""
    patron = re.compile(r"^(nada|abierta|puno)_s(\d+)_(\d+)\.pgm$")
    imgs, y, grupos, orden = [], [], [], []

    def agregar(nombre, ruta, datos):
        m = patron.match(os.path.basename(nombre))
        if not m:
            print("  salto (nombre raro):", nombre)
            return
        imgs.append(leer_pgm(ruta, datos))
        y.append(CLASES.index(m.group(1)))
        grupos.append(f"{m.group(1)}_s{m.group(2)}")
        orden.append(int(m.group(3)))

    for ruta in sorted(glob.glob(os.path.join(carpeta, "*.pgm"))):
        with open(ruta, "rb") as f:
            agregar(ruta, ruta, f.read())
    for ruta in sorted(glob.glob(os.path.join(carpeta, "*.zip"))):
        with zipfile.ZipFile(ruta) as z:
            for nombre in sorted(z.namelist()):
                agregar(nombre, ruta + ":" + nombre, z.read(nombre))
    return np.array(imgs, dtype=np.uint8), np.array(y), np.array(grupos), np.array(orden)


def dividir_por_sesiones(y, grupos, fraccion=0.25, semilla=7):
    rng = np.random.default_rng(semilla)
    val = np.zeros(len(y), dtype=bool)
    for c, nombre in enumerate(CLASES):
        sesiones = sorted(set(grupos[y == c]))
        if len(sesiones) < 3:
            sys.exit(f"Para '{nombre}' hay {len(sesiones)} sesion(es) y necesito al menos 3. "
                     "Haz mas rafagas (cada rafaga es una sesion) y cambia fondo o luz entre ellas.")
        n_val = max(1, int(round(len(sesiones) * fraccion)))
        for s in rng.choice(sesiones, n_val, replace=False):
            val |= grupos == s
    return ~val, val


# ----------------------------------------------------------------------------- red
def conv_adelante(x, w, b):
    """x: (N,H,W,Ci); w: (Co,3,3,Ci). Relleno con ceros, igual que la placa."""
    N, H, W, Ci = x.shape
    xp = np.pad(x, ((0, 0), (1, 1), (1, 1), (0, 0)))
    cols = np.stack([xp[:, dy:dy + H, dx:dx + W, :] for dy in range(3) for dx in range(3)], axis=3)
    cols = cols.reshape(N * H * W, 9 * Ci)
    out = cols @ w.reshape(w.shape[0], 9 * Ci).T + b
    return out.reshape(N, H, W, -1), cols


def conv_atras(dout, cols, w, x_forma):
    N, H, W, Ci = x_forma
    Co = w.shape[0]
    d = dout.reshape(N * H * W, Co)
    dw = (d.T @ cols).reshape(w.shape)
    db = d.sum(axis=0)
    dcols = (d @ w.reshape(Co, 9 * Ci)).reshape(N, H, W, 3, 3, Ci)
    dxp = np.zeros((N, H + 2, W + 2, Ci), dtype=dout.dtype)
    for dy in range(3):
        for dx in range(3):
            dxp[:, dy:dy + H, dx:dx + W, :] += dcols[:, :, :, dy, dx, :]
    return dxp[:, 1:-1, 1:-1, :], dw, db


def pool_adelante(x):
    N, H, W, C = x.shape
    xr = x.reshape(N, H // 2, 2, W // 2, 2, C).transpose(0, 1, 3, 2, 4, 5).reshape(N, H // 2, W // 2, 4, C)
    idx = xr.argmax(axis=3)
    out = np.take_along_axis(xr, idx[:, :, :, None, :], axis=3)[:, :, :, 0, :]
    return out, idx


def pool_atras(dout, idx, forma):
    N, H, W, C = forma
    dxr = np.zeros((N, H // 2, W // 2, 4, C), dtype=dout.dtype)
    np.put_along_axis(dxr, idx[:, :, :, None, :], dout[:, :, :, None, :], axis=3)
    return dxr.reshape(N, H // 2, W // 2, 2, 2, C).transpose(0, 1, 3, 2, 4, 5).reshape(N, H, W, C)


def forma_parametros():
    c1, c2, c3 = CANALES
    return {
        "c1w": (c1, 3, 3, 1), "c1b": (c1,),
        "c2w": (c2, 3, 3, c1), "c2b": (c2,),
        "c3w": (c3, 3, 3, c2), "c3b": (c3,),
        "dw": (len(CLASES), c3), "db": (len(CLASES),),
    }


def iniciar(semilla=1, tipo=np.float32):
    rng = np.random.default_rng(semilla)
    p = {}
    for nombre, forma in forma_parametros().items():
        if nombre.endswith("b"):
            p[nombre] = np.zeros(forma, dtype=tipo)
        else:
            entradas = int(np.prod(forma[1:]))
            p[nombre] = (rng.standard_normal(forma) * np.sqrt(2.0 / entradas)).astype(tipo)
    return p


def adelante(p, x, guardar=False):
    a1, col1 = conv_adelante(x, p["c1w"], p["c1b"])
    r1 = np.maximum(a1, 0)
    q1, i1 = pool_adelante(r1)
    a2, col2 = conv_adelante(q1, p["c2w"], p["c2b"])
    r2 = np.maximum(a2, 0)
    q2, i2 = pool_adelante(r2)
    a3, col3 = conv_adelante(q2, p["c3w"], p["c3b"])
    r3 = np.maximum(a3, 0)
    g = r3.mean(axis=(1, 2))
    z = g @ p["dw"].T + p["db"]
    z = z - z.max(axis=1, keepdims=True)
    e = np.exp(z)
    prob = e / e.sum(axis=1, keepdims=True)
    if not guardar:
        return prob
    return prob, dict(x=x, a1=a1, col1=col1, r1=r1, i1=i1, q1=q1, a2=a2, col2=col2, r2=r2, i2=i2,
                      q2=q2, a3=a3, col3=col3, r3=r3, g=g)


def perdida_y_gradientes(p, x, y, peso_clase):
    prob, c = adelante(p, x, guardar=True)
    N = len(y)
    w = peso_clase[y]
    perdida = float(-(w * np.log(prob[np.arange(N), y] + 1e-12)).sum() / N)
    dz = prob.copy()
    dz[np.arange(N), y] -= 1
    dz *= (w / N)[:, None]
    gr = {"dw": dz.T @ c["g"], "db": dz.sum(axis=0)}
    dg = dz @ p["dw"]
    _, H3, W3, _ = c["r3"].shape
    dr3 = np.broadcast_to(dg[:, None, None, :] / (H3 * W3), c["r3"].shape).copy()
    da3 = dr3 * (c["a3"] > 0)
    dq2, gr["c3w"], gr["c3b"] = conv_atras(da3, c["col3"], p["c3w"], c["q2"].shape)
    dr2 = pool_atras(dq2, c["i2"], c["r2"].shape)
    da2 = dr2 * (c["a2"] > 0)
    dq1, gr["c2w"], gr["c2b"] = conv_atras(da2, c["col2"], p["c2w"], c["q1"].shape)
    dr1 = pool_atras(dq1, c["i1"], c["r1"].shape)
    da1 = dr1 * (c["a1"] > 0)
    _, gr["c1w"], gr["c1b"] = conv_atras(da1, c["col1"], p["c1w"], c["x"].shape)
    return perdida, gr


# ----------------------------------------------------------------------------- entrenamiento
def aumentar(lote, rng):
    """lote: (N,H,W) con valores 0..1. Giros, desplazamientos, brillo, contraste y ruido."""
    salida = np.empty_like(lote)
    for i, img in enumerate(lote):
        if rng.random() < 0.5:
            img = img[:, ::-1]
        dy, dx = rng.integers(-5, 6), rng.integers(-6, 7)
        img = np.pad(img, ((5, 5), (6, 6)), mode="edge")[5 - dy:5 - dy + ALTO, 6 - dx:6 - dx + ANCHO]
        a, b = rng.uniform(0.65, 1.35), rng.uniform(-0.15, 0.15)
        salida[i] = img * a + b + rng.normal(0, 0.02, img.shape)
    return np.clip(salida, 0, 1)


def predecir(p, imgs, media, desv, lote=128):
    salida = []
    for i in range(0, len(imgs), lote):
        x = ((imgs[i:i + lote].astype(np.float32) / 255 - media) / desv)[..., None]
        salida.append(adelante(p, x))
    return np.concatenate(salida)


def entrenar(imgs, y, tr, va, epocas=40, lote=32, tasa=2e-3, semilla=1, mostrar=True):
    rng = np.random.default_rng(semilla)
    media = float(imgs[tr].mean() / 255)
    desv = float(imgs[tr].std() / 255)
    p = iniciar(semilla)
    cuentas = np.bincount(y[tr], minlength=len(CLASES)).astype(np.float32)
    peso_clase = (cuentas.sum() / (len(CLASES) * np.maximum(cuentas, 1))).astype(np.float32)
    m = {k: np.zeros_like(v) for k, v in p.items()}
    v = {k: np.zeros_like(x) for k, x in p.items()}
    paso, mejor, mejor_p = 0, -1.0, None
    idx = np.where(tr)[0]
    total = epocas * int(np.ceil(len(idx) / lote))
    t0 = time.time()
    for ep in range(epocas):
        rng.shuffle(idx)
        suma = 0.0
        for i in range(0, len(idx), lote):
            b = idx[i:i + lote]
            x = aumentar(imgs[b].astype(np.float32) / 255, rng)
            x = ((x - media) / desv)[..., None].astype(np.float32)
            perdida, gr = perdida_y_gradientes(p, x, y[b], peso_clase)
            suma += perdida * len(b)
            paso += 1
            lr = tasa * 0.5 * (1 + np.cos(np.pi * paso / total))
            for k in p:
                m[k] = 0.9 * m[k] + 0.1 * gr[k]
                v[k] = 0.999 * v[k] + 0.001 * gr[k] ** 2
                p[k] -= (lr * (m[k] / (1 - 0.9 ** paso)) / (np.sqrt(v[k] / (1 - 0.999 ** paso)) + 1e-8)).astype(np.float32)
        pred = predecir(p, imgs[va], media, desv).argmax(axis=1)
        recalls = [float((pred[y[va] == c] == c).mean()) for c in range(len(CLASES))]
        puntaje = min(recalls)
        if puntaje >= mejor:
            mejor, mejor_p = puntaje, {k: x.copy() for k, x in p.items()}
        if mostrar and (ep % 5 == 4 or ep == 0):
            print(f"  epoca {ep + 1:3d}  perdida {suma / len(idx):.3f}  "
                  f"validacion peor clase {puntaje:.3f}  ({time.time() - t0:.0f} s)")
    return mejor_p, media, desv


# ----------------------------------------------------------------------------- evaluacion
def disparos(probs, minimo=MINIMO, ventana=VENTANA, confianza=CONFIANZA):
    """Aplica la misma regla de la placa a una secuencia de fotos. Devuelve los gestos disparados."""
    historial, hechos, libre = [], [], True
    for pr in probs:
        c = int(pr.argmax())
        historial.append(c if (c != 0 and pr[c] >= confianza) else 0)
        historial = historial[-ventana:]
        votos = [historial.count(k) for k in (1, 2)]
        k = int(np.argmax(votos)) + 1
        if libre and votos[k - 1] >= minimo:
            hechos.append(k)
            libre = False
        elif not libre and historial.count(0) >= ventana - 1:
            libre = True
    return hechos


def evaluar(p, imgs, y, grupos, orden, va, media, desv):
    prob = predecir(p, imgs[va], media, desv)
    pred = prob.argmax(axis=1)
    yv = y[va]
    conf = np.zeros((len(CLASES), len(CLASES)), dtype=int)
    for a, b in zip(yv, pred):
        conf[a, b] += 1
    exactitud = float((pred == yv).mean())
    recalls = {CLASES[c]: float(conf[c, c] / max(conf[c].sum(), 1)) for c in range(len(CLASES))}

    gv = grupos[va]
    ov = orden[va]
    falsos, segundos, aciertos, sesiones_gesto = 0, 0.0, 0, 0
    for s in sorted(set(gv)):
        sel = np.where(gv == s)[0]
        sel = sel[np.argsort(ov[sel])]
        clase = CLASES.index(s.split("_s")[0])
        hechos = disparos(prob[sel])
        if clase == 0:
            falsos += len(hechos)
            segundos += len(sel) / CAMARA_FPS
        else:
            sesiones_gesto += 1
            aciertos += int(clase in hechos)
    return dict(exactitud=exactitud, recalls=recalls, conf=conf, n_val=int(va.sum()),
                falsos_por_hora=(falsos / segundos * 3600) if segundos else float("nan"),
                segundos_nada=segundos, aciertos=aciertos, sesiones_gesto=sesiones_gesto)


# ----------------------------------------------------------------------------- archivo del modelo
def fnv1a(datos):
    h = 2166136261
    for b in datos:
        h = ((h ^ b) * 16777619) & 0xFFFFFFFF
    return h


def guardar_modelo(ruta, p, media, desv):
    cuerpo = b"".join(p[k].astype("<f4").tobytes() for k in forma_parametros())
    n = len(cuerpo) // 4
    cabecera = b"GEST" + struct.pack("<HHHBBffI", VERSION, ANCHO, ALTO, len(CLASES), 0, media, desv, n)
    with open(ruta, "wb") as f:
        f.write(cabecera + cuerpo + struct.pack("<I", fnv1a(cuerpo)))
    return fnv1a(cuerpo)


def cargar_modelo(ruta):
    with open(ruta, "rb") as f:
        d = f.read()
    assert d[:4] == b"GEST", "no es un modelo de gestos"
    version, w, h, nc, _, media, desv, n = struct.unpack("<HHHBBffI", d[4:24])
    assert (version, w, h, nc) == (VERSION, ANCHO, ALTO, len(CLASES)), "modelo de otra version"
    cuerpo = d[24:24 + 4 * n]
    assert struct.unpack("<I", d[24 + 4 * n:28 + 4 * n])[0] == fnv1a(cuerpo), "archivo danado"
    flat = np.frombuffer(cuerpo, dtype="<f4").astype(np.float32)
    p, i = {}, 0
    for k, forma in forma_parametros().items():
        n_k = int(np.prod(forma))
        p[k] = flat[i:i + n_k].reshape(forma).copy()
        i += n_k
    assert i == n, "numero de parametros distinto"
    return p, media, desv


def escribir_ficha(ruta, res, n_fotos, hash_modelo):
    c = res["conf"]
    filas = "\n".join(f"| {CLASES[i]} | " + " | ".join(str(v) for v in c[i]) + " |" for i in range(len(CLASES)))
    texto = f"""# Modelo de gestos

Sale de `detector/gestos/entrenar_gestos.py`. Red convolucional de 3 capas, 4803 numeros, entrada de 80x60 en gris.

- Fotos usadas: {n_fotos}. Validación: {res['n_val']} fotos de sesiones que el modelo no vio.
- Exactitud en validación: {res['exactitud']:.1%}
- Recall por clase: {', '.join(f'{k} {v:.1%}' for k, v in res['recalls'].items())}
- Con la regla de la cámara (5 de 6 fotos con 80 %): gestos acertados {res['aciertos']} de {res['sesiones_gesto']} sesiones; falsos disparos por hora con la clase nada: {res['falsos_por_hora']:.1f} (sobre {res['segundos_nada']:.0f} s de video)
- Suma de comprobación del archivo: {hash_modelo:08x}

Matriz de confusión (filas: lo que era; columnas: lo que dijo el modelo):

| | {' | '.join(CLASES)} |
| --- | --- | --- | --- |
{filas}
"""
    with open(ruta, "w", encoding="utf-8") as f:
        f.write(texto)


# ----------------------------------------------------------------------------- autoprueba
def comprobar_gradientes():
    """Compara los gradientes de la red con derivadas numericas, en doble precision."""
    rng = np.random.default_rng(3)
    p = iniciar(5, np.float64)
    for k in p:
        if k.endswith("b"):
            p[k] += rng.normal(0, 0.1, p[k].shape)
    x = rng.random((4, 12, 16, 1))
    y = np.array([0, 1, 2, 1])
    peso = np.array([1.0, 1.3, 0.8])
    _, gr = perdida_y_gradientes(p, x, y, peso)
    peor = 0.0
    for k in p:
        for _ in range(6):
            i = tuple(rng.integers(0, s) for s in p[k].shape)
            viejo = p[k][i]
            h = 1e-5
            p[k][i] = viejo + h
            a, _ = perdida_y_gradientes(p, x, y, peso)
            p[k][i] = viejo - h
            b, _ = perdida_y_gradientes(p, x, y, peso)
            p[k][i] = viejo
            num = (a - b) / (2 * h)
            peor = max(peor, abs(num - gr[k][i]) / max(1e-7, abs(num) + abs(gr[k][i])))
    return peor


def fotos_inventadas(rng, n_sesiones=6, por_sesion=40):
    """Tres 'gestos' falsos: fondo con ruido, mancha redonda, mancha con rayas."""
    yy, xx = np.mgrid[0:ALTO, 0:ANCHO]
    imgs, y, grupos, orden = [], [], [], []
    for c, nombre in enumerate(CLASES):
        for s in range(n_sesiones):
            fondo = rng.uniform(40, 200)
            inclinacion = rng.uniform(-0.8, 0.8)
            for k in range(por_sesion):
                img = fondo + inclinacion * (xx - ANCHO / 2) + rng.normal(0, 6, (ALTO, ANCHO))
                if c:
                    cx, cy = ANCHO / 2 + rng.integers(-8, 9), ALTO / 2 + rng.integers(-6, 7)
                    r = rng.uniform(11, 15)
                    dist = np.hypot(xx - cx, yy - cy)
                    brillo = 90 if fondo < 120 else -90
                    img = img + brillo * (dist < r)
                    if c == 1:
                        ang = np.arctan2(yy - cy, xx - cx)
                        rayas = (np.cos(5 * ang) > 0.55) & (dist > r) & (dist < r + 15)
                        img = img + brillo * rayas
                imgs.append(np.clip(img, 0, 255).astype(np.uint8))
                y.append(c)
                grupos.append(f"{nombre}_s{s + 1:02d}")
                orden.append(k)
    return np.array(imgs), np.array(y), np.array(grupos), np.array(orden)


def autoprueba():
    print("1. Gradientes contra derivadas numericas")
    err = comprobar_gradientes()
    print(f"   error relativo peor: {err:.2e}", "OK" if err < 1e-5 else "MAL")
    if err >= 1e-5:
        sys.exit(1)
    print("2. Entrenamiento con fotos inventadas")
    rng = np.random.default_rng(11)
    imgs, y, grupos, orden = fotos_inventadas(rng)
    tr, va = dividir_por_sesiones(y, grupos)
    p, media, desv = entrenar(imgs, y, tr, va, epocas=30)
    res = evaluar(p, imgs, y, grupos, orden, va, media, desv)
    print(f"   exactitud {res['exactitud']:.1%}, gestos acertados {res['aciertos']}/{res['sesiones_gesto']}, "
          f"falsos disparos por hora {res['falsos_por_hora']:.1f}")
    ok = res["exactitud"] > 0.9 and res["aciertos"] == res["sesiones_gesto"]
    print("   ", "OK" if ok else "MAL")
    return 0 if ok else 1


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--fotos", default=os.path.join(AQUI, "fotos"))
    ap.add_argument("--salida", default=os.path.join(AQUI, "modelo"))
    ap.add_argument("--epocas", type=int, default=40)
    ap.add_argument("--autoprueba", action="store_true")
    a = ap.parse_args()
    if a.autoprueba:
        sys.exit(autoprueba())

    imgs, y, grupos, orden = cargar_fotos(a.fotos)
    if not len(imgs):
        sys.exit(f"No encontre fotos .pgm en {a.fotos}. Bajalas con la pagina de la camara.")
    cuentas = {n: int((y == i).sum()) for i, n in enumerate(CLASES)}
    print("Fotos:", cuentas)
    if min(cuentas.values()) == 0:
        sys.exit("Falta alguna clase. Necesito nada, abierta y puno.")
    tr, va = dividir_por_sesiones(y, grupos)
    print(f"Entrenamiento: {int(tr.sum())} fotos. Validacion: {int(va.sum())} fotos de sesiones aparte.")
    p, media, desv = entrenar(imgs, y, tr, va, epocas=a.epocas)
    res = evaluar(p, imgs, y, grupos, orden, va, media, desv)

    os.makedirs(a.salida, exist_ok=True)
    ruta = os.path.join(a.salida, "modelo_gestos.bin")
    h = guardar_modelo(ruta, p, media, desv)
    p2, m2, d2 = cargar_modelo(ruta)
    assert np.allclose(predecir(p, imgs[va][:20], media, desv), predecir(p2, imgs[va][:20], m2, d2), atol=1e-6)
    escribir_ficha(os.path.join(a.salida, "ficha_gestos.md"), res, len(imgs), h)

    print(f"\nExactitud en validacion: {res['exactitud']:.1%}")
    print("Recall:", {k: f"{v:.1%}" for k, v in res['recalls'].items()})
    print(f"Gestos acertados con la regla de la camara: {res['aciertos']} de {res['sesiones_gesto']} sesiones")
    print(f"Falsos disparos por hora (clase nada): {res['falsos_por_hora']:.1f}")
    print("Matriz de confusion (filas: real, columnas: dijo):\n", res["conf"])
    print("\nListo:", ruta)
    print("Subelo a la camara desde su pagina, en 'Modelo de gestos'.")


if __name__ == "__main__":
    main()
