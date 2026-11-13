# App del celular

Es una sola página web, sin librerías externas, que el devkit guarda comprimida y sirve en `http://detector.local`.

| Archivo | Qué es |
| --- | --- |
| `index.html` | La app: En vivo, Grabar, Datos, Modelo y Ajustes |
| `entrenamiento.js` | El reentrenamiento: la misma regresión logística de `detector/ml/entrenar.py`, la validación por minutos y la línea `M,...` que entiende el S3 |
| `empaquetar.py` | Mete `entrenamiento.js` en la página, la comprime y escribe `firmware/devkit_centinela/app_web.h` |

Después de cualquier cambio: `python empaquetar.py` y volver a subir el sketch del devkit.

Para reentrenar, la app junta las grabaciones del PC (`ventanas.csv`, que baja de GitHub la primera vez y guarda en el navegador) con lo que se haya etiquetado en el devkit. Con eso funciona también sin internet, conectado a la red propia del devkit.

Lo comparé contra scikit-learn con las grabaciones del 10 de noviembre: las probabilidades coinciden hasta la cuarta cifra cuando a scikit-learn se le pide converger con la misma precisión, y la clase es la misma en todas las ventanas. Entrenar tarda unos 30 ms en el navegador y la validación completa unos 150 ms. También probé que el código del S3 acepta el modelo que genera la app y lo clasifica igual.
