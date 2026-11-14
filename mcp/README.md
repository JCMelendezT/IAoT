# Servidor MCP del detector

Le permite a Claude leer el historial de eventos del detector. Es solo lectura: no cambia ajustes, no graba, no toca el modelo ni las redes. Habla con la API de `firmware/devkit_centinela`.

| Herramienta | Qué devuelve |
| --- | --- |
| `eventos_recientes` | Los últimos 10 eventos del devkit (alertas, cambios de WiFi, grabaciones). Están en su memoria RAM y se pierden si se reinicia |
| `historial_alertas(horas)` | De 1 a 24 horas de la caja negra, con los minutos seguidos de alerta unidos en episodios: cuándo empezó, cuánto duró, la probabilidad máxima de dron y qué etiqueta tenían. Sirve para encontrar falsas alarmas |

## Instalar

Python 3.10 o más. El PC y el devkit tienen que estar en la misma red.

```
pip install -r mcp/requirements.txt
claude mcp add --scope user detector -- python C:\Users\juanc\Desktop\IAoT\mcp\servidor.py http://172.20.10.9
claude mcp list
```

`--scope user` lo deja disponible desde cualquier carpeta. La dirección va como último argumento; no uso `--env` porque se traga el nombre del servidor y el comando falla.

La dirección es la IP que muestra la app en la tarjeta de red o el Monitor Serie. Si no se pone ninguna, usa la variable `DETECTOR_URL` o, si tampoco está, `http://detector.local`. Después se le puede preguntar a Claude cosas como "¿hubo alertas anoche?" o "¿cuáles de las alertas de hoy no tienen etiqueta?".

## Probar sin la placa

```
cd mcp
python simulador_devkit.py        # devkit falso en http://127.0.0.1:8099
```

y en otra terminal, con la dirección `http://127.0.0.1:8099`. El simulador tiene una falsa alarma ya etiquetada como fondo, una alerta de dron y un hueco sin datos.

## Límites

- Las horas salen en hora de Colombia (UTC-5).
- Si el devkit no tiene la hora (todavía no abriste la app desde que se encendió), el historial responde "sin hora".
- La caja negra tiene un tamaño fijo y sobrescribe lo más viejo. Cuántas horas caben lo dice el campo `almacen.horas_max` de `/api/estado` (la app lo muestra en "Memoria"); pedir más horas que esas no trae más.
