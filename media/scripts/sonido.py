"""Diseno de sonido sintetico para los videos (todo generado con numpy)."""
import numpy as np, wave

SR = 44100
rng = np.random.default_rng(7)


class Pista:
    def __init__(self, dur):
        self.n = int(SR * dur)
        self.x = np.zeros((self.n, 2))

    def add(self, sig, t0, g=1.0, pan=0.0):
        if sig.ndim == 1:
            sig = np.stack([sig * (1 - pan), sig * (1 + pan)], 1)
        i = int(t0 * SR)
        if i >= self.n:
            return
        j = min(self.n, i + len(sig))
        self.x[i:j] += sig[: j - i] * g

    def guardar(self, ruta, pico=0.9):
        y = self.x.copy()
        # compresion suave y normalizacion
        y = np.tanh(y * 1.3) / np.tanh(1.3)
        y *= pico / max(1e-9, np.abs(y).max())
        w = wave.open(ruta, 'wb'); w.setnchannels(2); w.setsampwidth(2); w.setframerate(SR)
        w.writeframes((y * 32767).astype('<i2').tobytes()); w.close()


def tt(d):
    return np.arange(int(d * SR)) / SR


def env(d, a, r):
    t = tt(d)
    return np.minimum(1, t / max(a, 1e-4)) * np.minimum(1, (d - t) / max(r, 1e-4))


def lp(s, a):
    """filtro pasa bajos de un polo (a pequeno = mas oscuro)"""
    y = np.empty_like(s); acc = 0.0
    if np.isscalar(a):
        a = np.full(len(s), a)
    for i in range(len(s)):
        acc += (s[i] - acc) * a[i]; y[i] = acc
    return y


def ruido(d):
    return rng.standard_normal(int(d * SR))


def saw(f, t):
    ph = np.cumsum(np.broadcast_to(f, t.shape)) / SR
    return 2 * (ph % 1) - 1


def boom(d=3.0, f0=42):
    t = tt(d); f = f0 + 70 * np.exp(-t * 7)
    s = np.sin(2 * np.pi * np.cumsum(f) / SR) * np.exp(-t * 1.3)
    s += lp(ruido(d), 0.05) * np.exp(-t * 4) * 1.2
    return np.tanh(s * 1.8)


def braam(d=3.5, f=55):
    t = tt(d); e = np.minimum(1, t / 0.08) * np.exp(-t * 0.7)
    s = sum(saw(f * m * (1 + 0.003 * k), t) for k, m in enumerate([1, 1.5, 2, 3]))
    s = lp(s, 0.04 + 0.06 * np.exp(-t * 2)) * e
    return np.tanh(s * 3) * 0.9


def latido(n=4, intervalo=0.9):
    out = np.zeros(int((n * intervalo + 0.5) * SR))
    for k in range(n):
        for dt, g in ((0, 1), (0.18, 0.7)):
            t = tt(0.35); s = np.sin(2 * np.pi * (55 + 30 * np.exp(-t * 25)) * t) * np.exp(-t * 14) * g
            i = int((k * intervalo + dt) * SR); out[i:i + len(s)] += s
    return out


def zumbido_dron(d, f0=185, sube=0.0, cerca=None):
    """helices: dientes de sierra con batido y ruido; cerca = curva 0..1 de proximidad"""
    t = tt(d)
    f = f0 * (1 + sube * t / d) * (1 + 0.012 * np.sin(2 * np.pi * 0.7 * t))
    s = 0.6 * saw(f, t) + 0.35 * saw(f * 2.01, t) + 0.2 * saw(f * 3.02, t)
    s *= 0.75 + 0.25 * np.sin(2 * np.pi * 31 * t)
    s = lp(s, 0.18) + lp(ruido(d), 0.25) * 0.35
    c = np.ones(len(t)) if cerca is None else np.interp(np.linspace(0, 1, len(t)), np.linspace(0, 1, len(cerca)), cerca)
    return s * env(d, 0.4, 0.4) * c


def subida(d=2.0):
    t = tt(d)
    s = lp(ruido(d), 0.01 + 0.4 * (t / d) ** 2) * (t / d) ** 2
    s += np.sin(2 * np.pi * np.cumsum(200 + 900 * (t / d) ** 2) / SR) * 0.15 * (t / d) ** 3
    return s * 1.4


def whoosh(d=0.8):
    t = tt(d); c = np.sin(np.pi * t / d) ** 2
    return lp(ruido(d), 0.02 + 0.3 * c) * c * 1.6


def estatica(d=0.3):
    return ruido(d) * env(d, 0.005, 0.05) * 0.25 * (rng.random(int(d * SR)) > 0.6)


def tic(d=0.05):
    t = tt(d); return np.sin(2 * np.pi * 2200 * t) * np.exp(-t * 90) * 0.4


def alarma(n=6, paso=0.22):
    out = np.zeros(int((n * paso + 0.3) * SR))
    for k in range(n):
        t = tt(paso * 0.85); f = 1300 if k % 2 == 0 else 950
        s = np.sign(np.sin(2 * np.pi * f * t)) * env(paso * 0.85, 0.005, 0.03) * 0.18
        i = int(k * paso * SR); out[i:i + len(s)] += s
    return out


def ping(f=1568):
    t = tt(0.9); return (np.sin(2 * np.pi * f * t) + 0.4 * np.sin(2 * np.pi * f * 2 * t)) * np.exp(-t * 5) * 0.35


def colchon(d, acordes, compas=4.0, g=0.12):
    out = np.zeros(int(d * SR))
    k = 0
    while k * compas < d:
        for f in acordes[k % len(acordes)]:
            dd = min(compas + 0.6, d - k * compas)
            t = tt(dd); s = sum(np.sin(2 * np.pi * f * m * t + m) / m for m in (1, 2, 3)) + 0.5 * np.sin(2 * np.pi * f * 1.004 * t)
            s *= env(dd, 1.2, 1.2) * g
            i = int(k * compas * SR); out[i:i + len(s)] += s[: len(out) - i]
        k += 1
    return out


def pulso(d, bpm=110, raiz=55, desde=0.0):
    out = np.zeros(int(d * SR)); b = 60 / bpm
    k = 0
    while desde + k * b < d - 0.5:
        t0 = desde + k * b
        t = tt(0.45); f = 48 + 90 * np.exp(-t * 30)
        kick = np.sin(2 * np.pi * np.cumsum(f) / SR) * np.exp(-t * 8) * 0.8
        i = int(t0 * SR); out[i:i + len(kick)] += kick[: len(out) - i]
        t = tt(0.06); hat = np.diff(ruido(0.06), prepend=0) * np.exp(-t * 60) * 0.12
        i = int((t0 + b / 2) * SR); out[i:i + len(hat)] += hat[: len(out) - i]
        t = tt(b * 0.9); bass = np.tanh(2 * np.sin(2 * np.pi * raiz * t)) * np.exp(-t * 4) * 0.25
        i = int(t0 * SR); out[i:i + len(bass)] += bass[: len(out) - i]
        k += 1
    return out
