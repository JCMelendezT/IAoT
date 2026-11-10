# Ficha del modelo: detector de drones

Generada por `ml/entrenar.py` el 2026-11-10.

- **Modelo:** regresion logistica con normalizacion, 3 clases (fondo, bluetooth, dron).
- **Rasgos:** 28 por ventana de 2 s (9 del WiFi y 19 del espectro nRF24).
- **Tamano:** 143 numeros (~572 bytes en float32).
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
| Bloques de 1 min (10 pliegues) | 99.2% | 0.990 | 0 en 0.33 h sin dron | 0 |
| Ultimo 30 % de cada grabacion | 100.0% | 1.000 | 0 en 0.10 h sin dron | 0 |

Matriz de confusion (bloques de 1 min; filas = realidad, columnas = modelo):

| | fondo | bluetooth | dron |
| --- | --- | --- | --- |
| **fondo** | 296 | 3 | 0 |
| **bluetooth** | 5 | 295 | 0 |
| **dron** | 0 | 1 | 534 |

```
              precision    recall  f1-score   support

       fondo      0.983     0.990     0.987       299
   bluetooth      0.987     0.983     0.985       300
        dron      1.000     0.998     0.999       535

    accuracy                          0.992      1134
   macro avg      0.990     0.990     0.990      1134
weighted avg      0.992     0.992     0.992      1134
```

## Limites (van en el informe)

- Todo se grabo el mismo dia, en el mismo sitio y a 1 m. No se sabe aun como responde a 3, 5 o 10 m, ni en otro lugar.
- Cada clase es una sola grabacion seguida: el modelo pudo aprender algo propio de esa hora (por ejemplo, cuanto trafico WiFi habia).
  El espectro solo, que no depende del trafico de la casa, llega a ~97 %, asi que la deteccion se apoya sobre todo en la banda del dron.
- Solo detecta el Neo 2 con su control en 2,4 GHz; en 5,8 GHz o solo con el celular no se ve.
