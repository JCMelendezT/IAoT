# Ficha del modelo: detector de drones

Sale de `detector/ml/entrenar.py` (2026-11-13).

- **Modelo:** regresion logistica con normalizacion, 3 clases (fondo, bluetooth, dron).
- **Rasgos:** 19 por ventana de 2 s (0 del WiFi y 19 del espectro nRF24).
- **Tamano:** 98 numeros (~392 bytes en float32).
- **Datos:** 1134 ventanas, una grabacion larga por clase, en el estudio, a 1 m, el 20261110.

| Grabacion | Estado | Ventanas |
| --- | --- | --- |
| bluetooth | na | 300 |
| dron-control | tierra | 300 |
| dron-control | vuelo | 235 |
| fondo | na | 299 |

## Resultados

| Prueba | Exactitud | F1 promedio | Falsas alarmas de dron (sueltas) | Con regla de 2 ventanas seguidas |
| --- | --- | --- | --- | --- |
| Bloques de 1 min (10 pliegues) | 97.4% | 0.973 | 3 en 0.33 h sin dron | 0 |
| Ultimo 30 % de cada grabacion | 99.1% | 0.991 | 0 en 0.10 h sin dron | 0 |

Matriz de confusion (bloques de 1 min; filas = realidad, columnas = modelo):

| | fondo | bluetooth | dron |
| --- | --- | --- | --- |
| **fondo** | 296 | 3 | 0 |
| **bluetooth** | 4 | 293 | 3 |
| **dron** | 4 | 15 | 516 |

```
              precision    recall  f1-score   support

       fondo      0.974     0.990     0.982       299
   bluetooth      0.942     0.977     0.959       300
        dron      0.994     0.964     0.979       535

    accuracy                          0.974      1134
   macro avg      0.970     0.977     0.973      1134
weighted avg      0.975     0.974     0.975      1134
```

## Limites (van en el informe)

- Todo se grabo el mismo dia, en el mismo sitio y a 1 m. No se sabe aun como responde a 3, 5 o 10 m, ni en otro lugar.
- Cada clase es una sola grabacion seguida: el modelo pudo aprender algo propio de esa hora.
  Paso con el trafico WiFi: con esos rasgos el modelo decia "poco trafico = dron" y de noche daba falsas alarmas.
  Por eso este modelo usa solo el espectro del nRF24 (USAR_RASGOS_WIFI = False en entrenar.py).
- Solo detecta el Neo 2 con su control en 2,4 GHz; en 5,8 GHz o solo con el celular no se ve.
