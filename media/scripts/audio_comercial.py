"""Sonido del video comercial v2 (58 s). Uso: python audio_comercial.py salida.wav"""
import sys, numpy as np
from sonido import *
D = 58.0; P = Pista(D)
P.add(latido(4, 0.85), 0.0, 0.9)
c = np.linspace(0.2, 1.0, 1000) ** 1.5
P.add(zumbido_dron(7.2, 175, 0.25, c), 0, 0.35, -0.2)
P.add(zumbido_dron(7.2, 205, 0.3, c), 0, 0.25, 0.3)
P.add(boom(), 0.6, 0.9)
for k in range(5): P.add(estatica(0.25), 1.2 + k * 1.1, 0.8, 0.5 * (-1) ** k)
P.add(braam(3.0, 49), 4.6, 0.8)
for k in range(14): P.add(tic(), 4.6 + k * 0.09, 0.6)
P.add(whoosh(1.0), 6.4, 0.9)
P.add(boom(2.5, 50), 7.0, 0.7)
P.add(zumbido_dron(10, 190, 0.08, np.linspace(0.6, 1.0, 100)), 7, 0.5)
P.add(lp(ruido(10), 0.015) * env(10, 1, 1) * 0.8, 7, 0.6)
P.add(latido(8, 0.65), 11.0, 0.9)
P.add(subida(2.2), 14.8, 0.8)
P.add(boom(3, 38), 17.0, 1.0); P.add(alarma(4, 0.2), 17.0, 0.8); P.add(estatica(0.6), 17.0, 1.0)
P.add(colchon(40, [(110, 130.8, 164.8), (87.3, 110, 130.8), (130.8, 164.8, 196), (98, 123.5, 146.8)], 4.0, 0.10), 18.0, 1.0)
P.add(pulso(34, 112, 55, 0.0), 18.0, 0.9)
for t0 in (18.0, 24.0, 32.0, 38.0, 46.0): P.add(whoosh(0.7), t0 - 0.45, 0.8); P.add(boom(2.0, 45), t0, 0.55)
P.add(alarma(6, 0.22), 26.4, 0.9)
P.add(ping(), 44.0, 1.0)
P.add(braam(4.5, 44), 52.0, 0.9); P.add(boom(4, 36), 52.0, 0.8)
fo = int(3.5 * SR); P.x[-fo:] *= np.linspace(1, 0, fo)[:, None]
P.guardar(sys.argv[1] if len(sys.argv) > 1 else 'audio_comercial.wav')
