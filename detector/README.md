# Datos y modelo del detector

Aquí está todo lo que va del dato crudo al modelo que corre en el S3.

| Carpeta | Contenido |
| --- | --- |
| `captura-datos/` | Capturador para el PC (`Capturar.bat`), registro de sesiones (`sesiones.csv`), grabaciones crudas (`raw/`, fuera de git) y la tabla de ventanas (`procesado/ventanas.csv`) |
| `ml/` | `rasgos.py` arma las ventanas y `entrenar.py` entrena, evalúa y exporta el modelo |
| `modelos/` | El modelo en C (`modelo_detector.h`), su ficha con resultados y 20 ventanas de prueba |
| `resultados/` | Matriz de confusión y tabla de detección por distancia |
| `informe/` | Informe final (pendiente) |

La propuesta original del proyecto está en [`../docs/curso/propuesta-detector.md`](../docs/curso/propuesta-detector.md). Algunas cosas cambiaron en el camino (por ejemplo, no hizo falta una red neuronal ni cuantizar a int8); el porqué está en la [bitácora](../docs/decisiones.md).

## Grabar datos desde el PC

1. Subir `firmware/nodo_captura` al S3 y desconectar el devkit, porque su WiFi a pocos centímetros ensucia la grabación.
2. Doble clic en `captura-datos/Capturar.bat`, escoger el puerto y conectar.
3. Escoger la clase y la distancia, y grabar. Cada grabación queda en `raw/` y se anota en `sesiones.csv`.

Las sesiones con "exploratoria", "prueba" o "no usar" en las notas no entran al entrenamiento.

Hoy también se puede grabar desde la app del celular, sin desconectar nada (ver `firmware/devkit_centinela`).
