"""Entrena la red de gestos con PyTorch (la que se recomienda). Mejor que entrenar_gestos.py.

Red: 4 capas convolucionales (8-16-32-48 canales) con BatchNorm. Al guardar, la BatchNorm se
"funde" dentro de cada convolucion, asi la camara solo hace conv + suma + ReLU.
Escribe el modelo en formato version 2 (la camara lee cuantas capas y canales tiene).

  python entrenar_gestos_torch.py                       # usa detector/gestos/fotos/*.zip
  python entrenar_gestos_torch.py --fotos CARPETA --epocas 80
  python entrenar_gestos_torch.py --dispositivo cpu     # por defecto usa la GPU si torch la ve
  python entrenar_gestos_torch.py --autoprueba

Necesita: pip install torch numpy pillow
"""
import argparse
import os
import struct
import sys
import time

import numpy as np
import torch
import torch.nn as nn
import torch.nn.functional as F

import entrenar_gestos as g

AQUI = g.AQUI
CLASES, ANCHO, ALTO = g.CLASES, g.ANCHO, g.ALTO
CANALES = (8, 16, 32, 48)
POOLS = len(CANALES) - 1
VERSION = 2


class Red(nn.Module):
    def __init__(self, canales=CANALES, media=0.5, desv=0.25):
        super().__init__()
        ch = (1,) + tuple(canales)
        self.canales = tuple(canales)
        self.media, self.desv = media, desv
        self.convs = nn.ModuleList([nn.Conv2d(ch[i], ch[i + 1], 3, padding=1, bias=False) for i in range(len(canales))])
        self.bns = nn.ModuleList([nn.BatchNorm2d(ch[i + 1]) for i in range(len(canales))])
        self.fc = nn.Linear(ch[-1], len(CLASES))

    def forward(self, x):
        x = (x - self.media) / self.desv
        for i in range(len(self.convs)):
            x = F.relu(self.bns[i](self.convs[i](x)))
            if i < len(self.convs) - 1:
                x = F.max_pool2d(x, 2)
        return self.fc(x.mean((2, 3)))


def aumentar(x):
    n = x.shape[0]
    volteo = torch.rand(n, device=x.device) < 0.5
    x = torch.where(volteo[:, None, None, None], x.flip(3), x)
    dy, dx = np.random.randint(-5, 6), np.random.randint(-6, 7)
    x = torch.roll(x, (dy, dx), (2, 3))
    a = torch.empty(n, 1, 1, 1, device=x.device).uniform_(0.65, 1.35)
    b = torch.empty(n, 1, 1, 1, device=x.device).uniform_(-0.15, 0.15)
    return (x * a + b + torch.randn_like(x) * 0.02).clamp(0, 1)


def entrenar(imgs, y, tr, va, epocas=80, lote=64, tasa=5e-3, dispositivo="cpu", semilla=1, mostrar=True):
    torch.manual_seed(semilla)
    np.random.seed(semilla)
    dev = torch.device(dispositivo)
    X = (torch.tensor(imgs, dtype=torch.float32)[:, None] / 255.0).to(dev)
    Y = torch.tensor(y).to(dev)
    media, desv = X[tr].mean().item(), X[tr].std().item()
    m = Red(CANALES, media, desv).to(dev)
    opt = torch.optim.AdamW(m.parameters(), lr=tasa, weight_decay=1e-4)
    idx = np.where(tr)[0]
    pasos = epocas * int(np.ceil(len(idx) / lote))
    sch = torch.optim.lr_scheduler.OneCycleLR(opt, tasa, total_steps=pasos)
    cnt = np.bincount(y[tr], minlength=len(CLASES))
    peso = torch.tensor(cnt.sum() / (len(CLASES) * np.maximum(cnt, 1)), dtype=torch.float32, device=dev)
    t0 = time.time()
    for e in range(epocas):
        m.train()
        np.random.shuffle(idx)
        for i in range(0, len(idx), lote):
            b = torch.tensor(idx[i:i + lote], device=dev)
            loss = F.cross_entropy(m(aumentar(X[b])), Y[b], weight=peso)
            opt.zero_grad()
            loss.backward()
            opt.step()
            sch.step()
        if mostrar and ((e + 1) % 10 == 0 or e == epocas - 1):
            m.eval()
            with torch.no_grad():
                pred = m(X[torch.tensor(np.where(va)[0], device=dev)]).argmax(1).cpu().numpy()
            print(f"  epoca {e + 1:3d}/{epocas}  perdida {loss.item():.3f}  exactitud {(pred == y[va]).mean():.1%}  {time.time() - t0:.0f} s", flush=True)
    m.eval()
    return m.cpu(), media, desv


def probabilidades(m, imgs):
    with torch.no_grad():
        X = torch.tensor(imgs, dtype=torch.float32)[:, None] / 255.0
        return torch.cat([F.softmax(m(X[i:i + 256]), 1) for i in range(0, len(X), 256)]).numpy()


def parametros_fundidos(m):
    """Lista de (pesos[Co][ky][kx][Ci], sesgo[Co]) con la BatchNorm ya dentro, y la capa final."""
    capas = []
    for conv, bn in zip(m.convs, m.bns):
        k = (bn.weight / torch.sqrt(bn.running_var + bn.eps)).detach()
        w = conv.weight.detach() * k[:, None, None, None]
        b = (bn.bias - bn.running_mean * k).detach()
        capas.append((w.permute(0, 2, 3, 1).contiguous().numpy(), b.numpy()))
    return capas, m.fc.weight.detach().numpy(), m.fc.bias.detach().numpy()


def guardar_modelo(ruta, m, media, desv):
    capas, dw, db = parametros_fundidos(m)
    cuerpo = b"".join(w.astype("<f4").tobytes() + b.astype("<f4").tobytes() for w, b in capas)
    cuerpo += dw.astype("<f4").tobytes() + db.astype("<f4").tobytes()
    n = len(cuerpo) // 4
    canales = bytes(m.canales) + bytes(8 - len(m.canales))
    cab = b"GEST" + struct.pack("<HHHBBffI", VERSION, ANCHO, ALTO, len(CLASES), len(m.canales), media, desv, n) + canales
    h = g.fnv1a(cuerpo)
    with open(ruta, "wb") as f:
        f.write(cab + cuerpo + struct.pack("<I", h))
    return h


def inferir_numpy(ruta, imgs):
    """Lee el .bin y lo calcula con numpy (para comprobar que lo guardado es lo mismo)."""
    d = open(ruta, "rb").read()
    version, w_, h_, nc, capas, media, desv, n = struct.unpack("<HHHBBffI", d[4:24])
    assert version == 2
    ch = list(d[24:24 + capas])
    flat = np.frombuffer(d[32:32 + 4 * n], dtype="<f4")
    out = []
    for im in imgs:
        x = ((im.astype(np.float32) / 255.0 - media) / desv)[:, :, None]
        p, ci = 0, 1
        for i, co in enumerate(ch):
            w = flat[p:p + co * 9 * ci].reshape(co, 3, 3, ci); p += co * 9 * ci
            b = flat[p:p + co]; p += co
            H, W = x.shape[:2]
            xp = np.pad(x, ((1, 1), (1, 1), (0, 0)))
            y_ = np.zeros((H, W, co), np.float32)
            for ky in range(3):
                for kx in range(3):
                    y_ += xp[ky:ky + H, kx:kx + W, :] @ w[:, ky, kx, :].T
            x = np.maximum(y_ + b, 0)
            if i < capas - 1:
                h2, w2 = H // 2, W // 2
                x = x[:h2 * 2, :w2 * 2].reshape(h2, 2, w2, 2, co).max((1, 3))
            ci = co
        gp = x.mean((0, 1))
        dw = flat[p:p + 3 * ci].reshape(3, ci); p += 3 * ci
        z = dw @ gp + flat[p:p + 3]
        e = np.exp(z - z.max())
        out.append(e / e.sum())
    return np.array(out)


def escribir_ficha(ruta, res, n_fotos, h, params):
    c = res["conf"]
    filas = "\n".join(f"| {CLASES[i]} | " + " | ".join(str(v) for v in c[i]) + " |" for i in range(len(CLASES)))
    texto = f"""# Modelo de gestos

Sale de `detector/gestos/entrenar_gestos_torch.py`. Red convolucional de {len(CANALES)} capas ({'-'.join(map(str, CANALES))} canales, BatchNorm fundida), {params} numeros, entrada de 80x60 en gris.

- Fotos usadas: {n_fotos}. Validación: {res['n_val']} fotos de sesiones que el modelo no vio.
- Exactitud en validación: {res['exactitud']:.1%}
- Recall por clase: {', '.join(f'{k} {v:.1%}' for k, v in res['recalls'].items())}
- Con la regla de la cámara (5 de 6 fotos con 80 %): gestos acertados {res['aciertos']} de {res['sesiones_gesto']} sesiones; falsos disparos por hora con la clase nada: {res['falsos_por_hora']:.1f} (sobre {res['segundos_nada']:.0f} s de video)
- Suma de comprobación del archivo: {h:08x}

Matriz de confusión (filas: lo que era; columnas: lo que dijo el modelo):

| | {' | '.join(CLASES)} |
| --- | --- | --- | --- |
{filas}
"""
    with open(ruta, "w", encoding="utf-8") as f:
        f.write(texto)


def evaluar(m, imgs, y, grupos, orden, va, media, desv):
    return g.evaluar(None, imgs, y, grupos, orden, va, media, desv, prob=probabilidades(m, imgs[va]))


def autoprueba():
    print("1. Entrenamiento con fotos inventadas")
    rng = np.random.default_rng(11)
    imgs, y, grupos, orden = g.fotos_inventadas(rng)
    tr, va = g.dividir_por_sesiones(y, grupos)
    m, media, desv = entrenar(imgs, y, tr, va, epocas=15, mostrar=False)
    res = evaluar(m, imgs, y, grupos, orden, va, media, desv)
    print(f"   exactitud {res['exactitud']:.1%}, gestos {res['aciertos']}/{res['sesiones_gesto']}")
    ok1 = res["exactitud"] > 0.9
    print("2. El archivo guardado da lo mismo que torch")
    ruta = os.path.join(os.environ.get("TMPDIR", "/tmp"), "prueba_gestos_torch.bin")
    guardar_modelo(ruta, m, media, desv)
    a, b = probabilidades(m, imgs[va][:8]), inferir_numpy(ruta, imgs[va][:8])
    err = float(np.abs(a - b).max())
    print(f"   diferencia maxima {err:.2e}", "OK" if err < 1e-4 else "MAL")
    return 0 if ok1 and err < 1e-4 else 1


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--fotos", default=os.path.join(AQUI, "fotos"))
    ap.add_argument("--salida", default=os.path.join(AQUI, "modelo"))
    ap.add_argument("--epocas", type=int, default=80)
    ap.add_argument("--dispositivo", default="auto", help="auto (usa la GPU si torch la ve), cpu o cuda")
    ap.add_argument("--autoprueba", action="store_true")
    a = ap.parse_args()
    if a.autoprueba:
        sys.exit(autoprueba())
    if a.dispositivo == "auto":
        a.dispositivo = "cuda" if torch.cuda.is_available() else "cpu"
    elif a.dispositivo == "cuda" and not torch.cuda.is_available():
        print("Torch no ve la GPU, uso la CPU.")
        a.dispositivo = "cpu"

    imgs, y, grupos, orden = g.cargar_fotos(a.fotos)
    if not len(imgs):
        sys.exit(f"No encontre fotos en {a.fotos}.")
    cuentas = {n: int((y == i).sum()) for i, n in enumerate(CLASES)}
    print("Fotos:", cuentas)
    if min(cuentas.values()) == 0:
        sys.exit("Falta alguna clase. Necesito nada, abierta y puno.")
    tr, va = g.dividir_por_sesiones(y, grupos)
    print(f"Entrenamiento: {int(tr.sum())} fotos. Validacion: {int(va.sum())} fotos de sesiones aparte. Dispositivo: {a.dispositivo}")
    m, media, desv = entrenar(imgs, y, tr, va, epocas=a.epocas, dispositivo=a.dispositivo)
    res = evaluar(m, imgs, y, grupos, orden, va, media, desv)

    os.makedirs(a.salida, exist_ok=True)
    ruta = os.path.join(a.salida, "modelo_gestos.bin")
    h = guardar_modelo(ruta, m, media, desv)
    dif = float(np.abs(probabilidades(m, imgs[va][:20]) - inferir_numpy(ruta, imgs[va][:20])).max())
    assert dif < 1e-3, f"el archivo no coincide con la red ({dif})"
    params = os.path.getsize(ruta) // 4 - 8 - 1
    escribir_ficha(os.path.join(a.salida, "ficha_gestos.md"), res, len(imgs), h, params)

    print(f"\nExactitud en validacion: {res['exactitud']:.1%}")
    print("Recall:", {k: f"{v:.1%}" for k, v in res['recalls'].items()})
    print(f"Gestos acertados con la regla de la camara: {res['aciertos']} de {res['sesiones_gesto']} sesiones")
    print(f"Falsos disparos por hora (clase nada): {res['falsos_por_hora']:.1f}")
    print("Matriz de confusion (filas: real, columnas: dijo):\n", res["conf"])
    print("\nListo:", ruta)
    print("Subelo a la camara desde su pagina, en 'Modelo de gestos'.")


if __name__ == "__main__":
    main()
