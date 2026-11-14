# devkit_centinela (ESP32 devkit)

La pasarela del detector. Recibe por cable lo que decide el S3, lo guarda en su memoria y sirve la app del celular. Si el laboratorio de AWS está encendido, también sube las lecturas a AWS IoT como en la Práctica 3.

## Subirlo

1. Copiar `secrets.h.example` como `secrets.h` y poner el WiFi de 2,4 GHz.
2. `certs.h` lo escribe `terraform apply` en `terraform/aws`. Sin AWS, cambiar `USAR_AWS` a 0 al inicio del sketch.
3. Arduino IDE: placa **ESP32 Dev Module** y *Partition Scheme* **"No OTA (2MB APP/2MB SPIFFS)"**, que deja más espacio para datos. Si se usa AWS, instalar la librería **PubSubClient**.
4. Subir. El Monitor Serie (115200) muestra la dirección para abrir en el celular.

## En el celular

En la casa: `http://detector.local` en Safari (si no carga, la IP que muestra el Monitor Serie). Las redes WiFi se manejan desde la app, en Ajustes → Redes WiFi: busca las redes cercanas, escoges una, pones la clave y queda guardada en la memoria del devkit (hasta 5, además de las de `secrets.h`). El devkit se une a la conocida con más señal. Para usar los datos del iPhone hay que activar Compartir Internet con "Maximizar compatibilidad" (el ESP32 solo ve 2,4 GHz), guardar esa red desde la app y dejar abierta la pantalla de Compartir Internet mientras el devkit se conecta. Si no ve ninguna red conocida, crea la red **CENTINELA-RF** (clave `centinela2026`) y la app queda en `http://192.168.4.1`, sin internet; cada 30 s, mientras nadie esté conectado a ella, vuelve a buscar. Cuando se conecta a una red, apaga CENTINELA-RF. Si una red guardada se cae por más de un minuto, la vuelve a abrir. La primera vez la app le pasa la hora al devkit. Para tenerla como app: Compartir → Agregar a inicio.

La app tiene cinco pestañas:

- **En vivo:** lo que dice el detector y las probabilidades de cada clase.
- **Grabar:** marca todo lo que se grabe durante un rato como fondo, bluetooth o dron, de 2 minutos a toda la noche.
- **Datos:** línea de tiempo de lo grabado; permite etiquetar un rango (por ejemplo, "anoche no hubo dron") y descargar el CSV.
- **Modelo:** reentrena en el celular con las grabaciones del PC más lo etiquetado, muestra la exactitud y las falsas alarmas, y manda el modelo al S3.
- **Ajustes:** cuántas ventanas hacen falta para alertar y apagar la alerta, el sonido, y un botón para probarlo.

## Caja negra

Cada ventana ocupa 82 bytes: la hora, la clase que dio el modelo, la probabilidad de dron, la etiqueta y los 35 rasgos. Con la partición recomendada caben unas 12 horas; cuando se llena, borra lo más viejo.

## API

| Ruta | Qué hace |
| --- | --- |
| `GET /api/estado` | Estado en vivo, modelo, ajustes, memoria y eventos |
| `GET /api/historial?horas=12` | Resumen por minuto para la línea de tiempo |
| `GET /api/datos?etiquetados=1` | CSV con los rasgos (mismos nombres de columna que `ventanas.csv`) |
| `POST /api/grabar?clase=fondo&minutos=30` | Empieza una grabación; `clase=ninguna` la detiene |
| `POST /api/etiquetar?desde=&hasta=&clase=` | Etiqueta un rango de tiempo (segundos UTC) |
| `POST /api/modelo` | Recibe la línea `M,...` y se la pasa al S3 |
| `POST /api/modelo_fabrica` | Vuelve al modelo de fábrica |
| `POST /api/ajuste?clave=&valor=` | Cambia un ajuste del S3 |
| `POST /api/borrar_datos` | Vacía la caja negra |
| `POST /api/hora?epoch=` | Le da la hora al devkit cuando no tiene internet |

La app está en `app/`. Si se cambia, hay que correr `python empaquetar.py` allí para regenerar `app_web.h` y volver a subir este sketch.
