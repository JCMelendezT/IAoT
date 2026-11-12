# devkit_centinela — pasarela del detector con app para el iPhone

El ESP32 devkit recibe por cable todo lo que decide el S3 (`nodo_detector`), lo guarda en su memoria (caja negra, unas 9 a 12 horas) y sirve una **app web** para el celular. El S3 se queda callado (sin BLE ni WiFi propio) para no ensuciar la banda que está midiendo.

```
S3 (nodo_detector) --GPIO17->GPIO16--> devkit --WiFi--> iPhone (http://detector.local)
                   <-GPIO18<-GPIO17--         \--> AWS IoT (si el Learner Lab está prendido)
```

## Qué se hace desde la app

| Pestaña | Para qué |
| --- | --- |
| En vivo | Estado (despejado, bluetooth, dron, alerta), probabilidades, eventos |
| Grabar | Sesión etiquetada: fondo, bluetooth o dron, de 2 min a toda la noche |
| Datos | Línea de tiempo de la caja negra, etiquetar un rato ("anoche no hubo dron"), descargar CSV |
| Modelo | Reentrenar en el celular (grabaciones del PC + lo etiquetado) y mandar el modelo al S3 |
| Ajustes | Ventanas para alertar y apagar, sonido, probar el parlante |

## Cómo subirlo

1. Copia `secrets.h.example` como `secrets.h` y pon tu WiFi de 2,4 GHz.
2. `certs.h` (AWS) lo escribe `terraform apply`. Si no usas AWS, pon `#define USAR_AWS 0` al inicio del sketch.
3. Arduino IDE: placa **ESP32 Dev Module**, **Partition Scheme: "No OTA (2MB APP/2MB SPIFFS)"** (más horas de caja negra; con la de fábrica también compila). Librería **PubSubClient** si usas AWS.
4. Sube el sketch. En el Monitor Serie (115200) sale la dirección para el celular.

## En el iPhone

- En casa: abre **http://detector.local** en Safari. Si no carga, usa la IP que muestra el Monitor Serie.
- Sin el WiFi de la casa (por ejemplo, en la universidad): el devkit crea la red **CENTINELA-RF** (clave `centinela2026`). Conéctate y abre **http://192.168.4.1**. La app le pasa la hora al devkit sola.
- Para que quede como app: botón **Compartir → Agregar a inicio**.

## API (por si quieres usarla desde otro lado)

| Ruta | Qué hace |
| --- | --- |
| `GET /api/estado` | Estado en vivo, modelo, ajustes, caja negra, eventos |
| `GET /api/historial?horas=12` | Resumen por minuto para la línea de tiempo |
| `GET /api/datos?etiquetados=1` | CSV con los 35 rasgos de cada ventana (mismos nombres que `ventanas.csv`) |
| `POST /api/grabar?clase=fondo&minutos=30` | Sesión etiquetada (`clase=ninguna` la para) |
| `POST /api/etiquetar?desde=&hasta=&clase=` | Etiqueta un rango de tiempo (epoch UTC) |
| `POST /api/modelo` (texto `M,...`) | Manda un modelo al S3 (formato en `nodo_detector/modelo_activo.h`) |
| `POST /api/modelo_fabrica`, `POST /api/ajuste?clave=&valor=`, `POST /api/borrar_datos`, `POST /api/hora?epoch=` | |

La app está en `Lab_ESP32/app_centinela/`; si la cambias, corre `python empaquetar.py` para regenerar `app_web.h`.
