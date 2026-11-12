# app_centinela — app del celular (la sirve el devkit)

Una sola página web, sin librerías externas, que el `devkit_centinela` guarda comprimida en su memoria.

| Archivo | Qué es |
| --- | --- |
| `index.html` | La app: pestañas En vivo, Grabar, Datos, Modelo y Ajustes |
| `entrenamiento.js` | Reentrenamiento en el celular: igual a `ml/entrenar.py` (regresión logística, pesos balanceados, L2 con C = 1, validación por bloques de 1 minuto) y la línea `M,...` que entiende el S3 |
| `empaquetar.py` | Mete `entrenamiento.js` en la página, la comprime y escribe `firmware/devkit_centinela/app_web.h` |

Después de cambiar algo: `python empaquetar.py` y volver a subir el devkit.

**Comprobado (2026-11-12):** con las grabaciones del 10 nov, el entrenamiento en JavaScript da las mismas probabilidades que scikit-learn con tolerancia estricta (diferencia máxima 6e-5) y la misma clase en las 1134 ventanas; tarda unos 30 ms en entrenar y 150 ms en validar. El código del S3 (`modelo_activo.h`) acepta la línea `M,...` que genera la app, rechaza una línea alterada y clasifica igual que la app (diferencia 4e-7).

Las grabaciones base se bajan una vez de GitHub (`ventanas.csv`) y quedan guardadas en el navegador, así que reentrenar después funciona aunque el celular esté conectado a la red propia del devkit, sin internet.
