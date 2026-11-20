"""Convierte fotos del dataset publico HaGRID al formato de la camara (80x60 en gris).

HaGRID tiene fotos de personas haciendo gestos, con 'palm' (mano abierta), 'fist' (puno) y
'no_gesture'. Este programa:
  - recorre la carpeta donde descomprimiste el dataset,
  - recorta alrededor de la mano (si encuentra los archivos de anotaciones .json) y la pasa a 80x60 en gris,
  - agrupa las fotos por persona (user_id): cada persona es una "sesion", asi el entrenador
    valida con personas que no vio,
  - escribe 3 archivos .zip en fotos/ que el entrenador lee igual que los de la camara.

Uso:
    python preparar_publico.py CARPETA_DEL_DATASET
    python preparar_publico.py CARPETA --max-por-clase 1500 --salida fotos
Necesita: pip install numpy pillow
Dataset: https://huggingface.co/datasets/cj-mills/hagrid-sample-30k-384p  (CC BY-SA 4.0, Kapitanov et al., WACV 2024)
"""
import argparse
import collections
import json
import os
import random
import re
import sys
import zipfile

from PIL import Image

ANCHO, ALTO = 80, 60
PRINCIPALES = {"palm": "abierta", "fist": "puno", "no_gesture": "nada"}
# Otros gestos de HaGRID que sirven como "nada" (se evitan los que se parecen a mano abierta o puno)
OTROS_NADA = {"one", "peace", "peace_inverted", "ok", "call", "rock", "two_up", "two_up_inverted",
              "mute", "three", "three2"}
EXCLUIDOS = {"stop", "stop_inverted", "four", "like", "dislike"}
EXT = (".jpg", ".jpeg", ".png")


def etiqueta_de_ruta(ruta):
    """Busca en las carpetas de la ruta un nombre de gesto de HaGRID (quita prefijos como train_val_)."""
    for parte in reversed(ruta.replace("\\", "/").lower().split("/")[:-1]):
        parte = re.sub(r"^(train_val_|train_|val_|test_)", "", parte)
        if parte in PRINCIPALES or parte in OTROS_NADA or parte in EXCLUIDOS:
            return parte
    return None


def cargar_anotaciones(raiz):
    """Une todos los .json de anotaciones: clave (nombre de la foto sin extension) -> datos."""
    ann = {}
    for base, _, archivos in os.walk(raiz):
        for a in archivos:
            if not a.lower().endswith(".json"):
                continue
            try:
                with open(os.path.join(base, a), encoding="utf-8") as f:
                    datos = json.load(f)
            except Exception:
                continue
            if isinstance(datos, dict):
                for k, v in datos.items():
                    if isinstance(v, dict) and "bboxes" in v:
                        ann[k] = v
    return ann


def recortar(img, anot, rng):
    """Recorte 4:3 alrededor de la mano para que se vea de cerca, como en la camara."""
    w, h = img.size
    caja = None
    if anot and anot.get("bboxes"):
        caja = max(anot["bboxes"], key=lambda b: b[2] * b[3])
    if caja is None:
        lado_w = min(w, h * 4 / 3)
        x0 = (w - lado_w) / 2
        return img.crop((int(x0), 0, int(x0 + lado_w), int(lado_w * 0.75)))
    cx, cy = (caja[0] + caja[2] / 2) * w, (caja[1] + caja[3] / 2) * h
    cw = max(caja[2] * w, caja[3] * h * 4 / 3)
    ancho = cw * rng.uniform(2.2, 4.5)               # la mano ocupa entre 1/4 y 1/2 del ancho
    ancho = min(ancho, w, h * 4 / 3)
    alto = ancho * 0.75
    x0 = min(max(cx - ancho / 2, 0), w - ancho)
    y0 = min(max(cy - alto / 2, 0), h - alto)
    return img.crop((int(x0), int(y0), int(x0 + ancho), int(y0 + alto)))


def recorte_sin_mano(img, anot, rng):
    """Recorte de la persona o el fondo sin ninguna mano de gesto adentro (la clase 'nada')."""
    w, h = img.size
    cajas = [(b[0] * w, b[1] * h, (b[0] + b[2]) * w, (b[1] + b[3]) * h) for b in (anot or {}).get("bboxes", [])]
    if not cajas:
        return None
    base = min(w, h * 4 / 3)
    for _ in range(40):
        ancho = base * rng.uniform(0.35, 0.8)
        alto = ancho * 0.75
        x0, y0 = rng.uniform(0, w - ancho), rng.uniform(0, h - alto)
        libre = True
        for (a, b, c, d) in cajas:
            mx, my = (c - a) * 0.15, (d - b) * 0.15            # margen alrededor de la mano
            if x0 < c + mx and x0 + ancho > a - mx and y0 < d + my and y0 + alto > b - my:
                libre = False
                break
        if libre:
            return img.crop((int(x0), int(y0), int(x0 + ancho), int(y0 + alto)))
    return None


def a_pgm(img):
    g = img.convert("L").resize((ANCHO, ALTO), Image.BOX)
    return b"P5\n%d %d\n255\n" % (ANCHO, ALTO) + g.tobytes()


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("carpeta", help="carpeta donde descomprimiste HaGRID")
    ap.add_argument("--salida", default=os.path.join(os.path.dirname(os.path.abspath(__file__)), "fotos"))
    ap.add_argument("--max-por-clase", type=int, default=1500)
    ap.add_argument("--max-por-persona", type=int, default=25)
    ap.add_argument("--sin-otros", action="store_true", help="'nada' sin gestos distintos como negativos dificiles")
    ap.add_argument("--nada-sin-mano", type=float, default=0.7,
                    help="parte de 'nada' hecha con recortes sin mano (el resto: otros gestos). Defecto 0.7")
    ap.add_argument("--semilla", type=int, default=1)
    a = ap.parse_args()
    rng = random.Random(a.semilla)

    print("Buscando fotos en", a.carpeta)
    ann = cargar_anotaciones(a.carpeta)
    print("  anotaciones encontradas:", len(ann))
    candidatas = collections.defaultdict(list)       # clase nuestra -> [(ruta, clave, etiqueta)]
    todas = []                                       # cualquier foto con mano anotada (para 'nada' sin mano)
    for base, _, archivos in os.walk(a.carpeta):
        for nombre in archivos:
            if not nombre.lower().endswith(EXT):
                continue
            ruta = os.path.join(base, nombre)
            et = etiqueta_de_ruta(ruta)
            if et is None:
                continue
            todas.append((ruta, os.path.splitext(nombre)[0], et))
            if et in EXCLUIDOS:
                continue
            if et in PRINCIPALES:
                clase = PRINCIPALES[et]
            elif not a.sin_otros:
                clase = "nada"
            else:
                continue
            candidatas[clase].append((ruta, os.path.splitext(nombre)[0], et))
    if not candidatas:
        sys.exit("No encontre fotos de palm, fist ni no_gesture. Revisa la carpeta (deben estar en subcarpetas con esos nombres).")

    os.makedirs(a.salida, exist_ok=True)
    sin_persona = 0
    for clase in ("abierta", "puno", "nada"):
        lista = candidatas.get(clase, [])
        if not lista:
            print(f"  AVISO: no hay fotos para '{clase}'")
            continue
        rng.shuffle(lista)
        # 'nada': no_gesture es lo principal; los otros gestos completan hasta 40 %
        modo = {}
        if clase == "nada":
            ng = [x for x in lista if x[2] == "no_gesture"]
            otros = [x for x in lista if x[2] != "no_gesture"]
            n_sm = int(a.max_por_clase * a.nada_sin_mano)
            pool = list(todas)
            rng.shuffle(pool)
            sin_mano = [(r, k, e) for (r, k, e) in pool if (ann.get(k) or {}).get("bboxes")][:n_sm]
            for r, k, e in sin_mano:
                modo[r] = "sin_mano"
            resto = a.max_por_clase - len(sin_mano) - len(ng)
            lista = ng + sin_mano + (otros[:max(resto, 0)] if not a.sin_otros else [])
            rng.shuffle(lista)
        por_persona = collections.defaultdict(list)
        for i, (ruta, clave, et) in enumerate(lista):
            persona = (ann.get(clave) or {}).get("user_id")
            if persona is None:
                persona = "bloque%d" % (i // 30)       # sin anotaciones: bloques (puede inflar la exactitud)
                sin_persona += 1
            if len(por_persona[persona]) < a.max_por_persona:
                por_persona[persona].append((ruta, clave))
        escogidas = [(p, r, k) for p, fs in por_persona.items() for r, k in fs][: a.max_por_clase]
        ids = {}
        zpath = os.path.join(a.salida, f"publico_{clase}.zip")
        n = 0
        cuenta = collections.Counter()
        with zipfile.ZipFile(zpath, "w", zipfile.ZIP_STORED) as z:
            for persona, ruta, clave in escogidas:
                try:
                    with Image.open(ruta) as im:
                        im.load()
                        rgb = im.convert("RGB")
                        if modo.get(ruta) == "sin_mano":
                            rec = recorte_sin_mano(rgb, ann.get(clave), rng)
                            if rec is None:
                                continue
                        else:
                            rec = recortar(rgb, ann.get(clave), rng)
                        pgm = a_pgm(rec)
                except Exception as e:
                    print("  salto", ruta, e)
                    continue
                sid = ids.setdefault(persona, 100 + len(ids))
                cuenta[sid] += 1
                z.writestr(f"{clase}_s{sid:03d}_{cuenta[sid]:04d}.pgm", pgm)
                n += 1
        print(f"  {clase}: {n} fotos de {len(ids)} personas -> {zpath}")
    if sin_persona:
        print("\nAVISO: faltaron anotaciones (user_id) para algunas fotos; se agruparon por bloques.\n"
              "El entrenador puede dar una exactitud mas alta que la real. Busca los .json del dataset.")
    print("\nSiguiente: python entrenar_gestos.py")


if __name__ == "__main__":
    main()
