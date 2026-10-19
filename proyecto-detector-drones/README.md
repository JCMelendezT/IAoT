# Detector de drones por radiofrecuencia con IA en el borde

Diseño completo (problema, alcance, hardware, arquitectura, captura, modelos, evaluación y plan): `../Recursos de clase/Detector de drones por radiofrecuencia con IA en el borde.md`.

Este directorio guarda lo que se produce al ejecutarlo:

| Carpeta | Contenido |
| --- | --- |
| `captura-datos/` | `sesiones.csv` (registro versionado), `raw/` (crudo, ignorado por git), `procesado/` |
| `ml/` | Extracción de rasgos, entrenamiento, cuantización a int8 |
| `modelos/` | Modelos exportados con su ficha de métricas |
| `resultados/` | Curva de detección vs. distancia (2, 5, 10, 20, 30 m), falsas alarmas por hora, latencia, tamaño del modelo |
| `informe/` | Informe final y presentación |

Decisiones y cambios de plan: `decisiones.md`.
