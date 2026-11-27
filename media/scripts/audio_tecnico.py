"""Sonido del video tecnico v2 (165 s). Uso: python audio_tecnico.py salida.wav"""
import sys, numpy as np
from sonido import *
D = 165.0; P = Pista(D)
P.add(latido(5, 0.85), 0.0, 0.9)
c = np.linspace(0.2, 1.0, 1000) ** 1.5
P.add(zumbido_dron(8.2, 175, 0.25, c), 0, 0.35, -0.2)
P.add(zumbido_dron(8.2, 205, 0.3, c), 0, 0.25, 0.3)
P.add(boom(), 0.6, 0.9)
for k in range(6): P.add(estatica(0.25), 1.2 + k * 1.1, 0.8, 0.5 * (-1) ** k)
P.add(braam(2.5, 49), 3.0, 0.7)
for k in range(14): P.add(tic(), 3.0 + k * 0.09, 0.6)
P.add(boom(2.5, 46), 5.5, 0.7)
P.add(whoosh(1.0), 7.4, 0.9)
P.add(braam(4, 44), 8.0, 0.8)
P.add(zumbido_dron(16, 190, 0.05, np.linspace(0.5, 1.0, 100)), 8, 0.45)
P.add(lp(ruido(16), 0.015) * env(16, 1, 1) * 0.8, 8, 0.5)
P.add(latido(9, 0.7), 17.5, 0.85)
P.add(subida(2.2), 21.8, 0.8)
P.add(boom(3, 38), 24.0, 1.0); P.add(alarma(4, 0.2), 24.0, 0.8); P.add(estatica(0.6), 24.0, 1.0)
P.add(colchon(140, [(110, 130.8, 164.8), (87.3, 110, 130.8), (130.8, 164.8, 196), (98, 123.5, 146.8)], 4.0, 0.09), 25.0, 1.0)
P.add(pulso(130, 100, 55, 0.0), 25.0, 0.55)
for t0 in (25, 35, 49, 63, 75, 84, 96, 108, 121, 133, 147, 157): P.add(whoosh(0.7), t0 - 0.45, 0.6); P.add(boom(2.0, 45), t0, 0.45)
P.add(alarma(6, 0.22), 88.0, 0.8)
P.add(ping(), 141.5, 1.0)
P.add(braam(5, 44), 157.0, 0.8)
fo = int(4 * SR); P.x[-fo:] *= np.linspace(1, 0, fo)[:, None]
P.guardar(sys.argv[1] if len(sys.argv) > 1 else 'audio_tecnico.wav')
