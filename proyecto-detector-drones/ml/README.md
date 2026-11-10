# ml — del dato crudo al modelo del ESP32

Dos pasos, en este orden (Python 3 con `numpy`, `pandas`, `scikit-learn` y `matplotlib`):

```bash
cd proyecto-detector-drones/ml
python rasgos.py      # raw/*.txt  ->  captura-datos/procesado/ventanas.csv
python entrenar.py    # ventanas.csv  ->  modelos/modelo_detector.h + ficha + matriz
```

| Archivo | Qué hace |
| --- | --- |
| `rasgos.py` | Una fila por ventana de 2 s (armada con el reloj del ESP32, igual que el firmware): 9 números del WiFi y el espectro del nRF24 resumido en bandas de 6 MHz más 5 medidas. Junta `dron-control` tierra y vuelo en la clase `dron`. |
| `entrenar.py` | Regresión logística (3 clases). Evalúa por bloques de 1 minuto y con el último 30 % de cada grabación, entrena con todo y exporta a C. |

Salidas:

- `modelos/modelo_detector.h`: pesos y la función `detectorClasificar(x, prob)` para Arduino. Sin librerías extra. Se copia también a `Lab_ESP32/firmware/nodo_detector/`, el firmware que lo usa.
- `modelos/ficha_modelo.md` y `metricas.json`: resultados y límites.
- `modelos/prueba_rasgos.csv` y `prueba_prob.csv`: 20 ventanas con la respuesta esperada, para comprobar el firmware.
- `resultados/matriz_confusion.png`.

Para reentrenar con nuevas grabaciones basta con dejarlas en `captura-datos/raw/` y volver a correr los dos pasos.
