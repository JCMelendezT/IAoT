# Modelo de gestos

Sale de `detector/gestos/entrenar_gestos_torch.py`. Red convolucional de 4 capas (8-16-32-48 canales, BatchNorm fundida), 19907 numeros, entrada de 80x60 en gris.

- Fotos usadas: 4490. Validación: 1146 fotos de sesiones que el modelo no vio.
- Exactitud en validación: 84.6%
- Recall por clase: nada 79.9%, abierta 94.0%, puno 80.3%
- Con la regla de la cámara (5 de 6 fotos con 80 %): gestos acertados 7 de 497 sesiones; falsos disparos por hora con la clase nada: 0.0 (sobre 96 s de video)
- Suma de comprobación del archivo: fadca6fe

Matriz de confusión (filas: lo que era; columnas: lo que dijo el modelo):

| | nada | abierta | puno |
| --- | --- | --- | --- |
| nada | 306 | 40 | 37 |
| abierta | 14 | 345 | 8 |
| puno | 70 | 8 | 318 |
