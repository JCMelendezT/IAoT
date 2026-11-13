# Entrenamiento

Necesita Python 3 con numpy, pandas, scikit-learn y matplotlib.

```bash
cd detector/ml
python rasgos.py      # captura-datos/raw/*.txt  ->  captura-datos/procesado/ventanas.csv
python entrenar.py    # ventanas.csv  ->  modelos/modelo_detector.h, ficha y matriz de confusión
```

`rasgos.py` convierte cada grabación en ventanas de 2 s. Por cada ventana guarda los 9 números del WiFi y el espectro del nRF24 resumido en 21 tramos de 6 MHz más 5 medidas. Las filas del nRF24 se asignan a cada ventana con el reloj del ESP32, igual que hace el firmware.

`entrenar.py` entrena una regresión logística con las tres clases (fondo, bluetooth y dron, sumando tierra y vuelo). Usa solo los rasgos del espectro; con `USAR_RASGOS_WIFI = True` vuelve a incluir los del WiFi, pero con ellos el modelo daba falsas alarmas de noche. La evaluación nunca usa el mismo minuto para entrenar y para probar. Al final exporta el modelo a C y lo copia también a `firmware/nodo_detector/`, así que basta con volver a subir el sketch.

Para revisar que la placa calcula lo mismo que Python está `firmware/nodo_detector/prueba_pc/`.
