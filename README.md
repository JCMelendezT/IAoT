# Detector de drones por radiofrecuencia

Proyecto de la materia Inteligencia Artificial aplicada al IoT (Especialización en IA, UAO, 2026-2). Es un detector pasivo: un ESP32-S3 escucha la banda de 2,4 GHz con un nRF24L01+ y decide dentro de la misma placa, con un modelo de menos de 1 KB, si lo que oye es el control de un dron. No depende de internet para alertar y no transmite nada para interferir.

Las tres prácticas del curso las hice alrededor del mismo detector, así que todo vive en este repositorio.

## Cómo funciona

El nRF24 barre 126 canales (2400 a 2525 MHz) unas cuatro veces por segundo. Cada 2 segundos el S3 resume esas filas en 19 números: la energía en cada tramo de 6 MHz y unas medidas de la banda donde aparece el dron (2423 a 2456 MHz). Con eso una regresión logística decide entre tres clases: fondo, bluetooth o dron. Si sale dron en dos ventanas seguidas, se enciende la alerta: el LED de la placa se pone rojo, suena el parlante y el S3 avisa por cable al ESP32 devkit.

El devkit es el que habla con el mundo. Guarda cada ventana en su memoria, sirve una app web para el celular desde la que grabo datos etiquetados y reentreno el modelo, y sube las lecturas a AWS cuando el laboratorio está encendido.

Diagramas (abren en el navegador):

- [Sistema completo](docs/diagramas/sistema.html)
- [Cómo decide en cada ventana](docs/diagramas/decision.html)
- [Ciclo de reentrenamiento desde el celular](docs/diagramas/reentrenamiento.html)

## Resultados

| Medida | Valor | Notas |
| --- | --- | --- |
| Exactitud | 97,4 % | Validación separando bloques de 1 minuto (10 pliegues) |
| Exactitud en datos posteriores | 99,1 % | Entrenando con el primer 70 % de cada grabación y probando con el resto |
| Falsas alarmas | 0 | Sobre las grabaciones, con la regla de 2 ventanas seguidas |
| Alcance en interiores | 5 m con el dron en tierra, 8 m volando | Prueba del 10 de noviembre en la casa |
| Tamaño del modelo | 98 números (392 bytes) | No necesitó cuantización |

La matriz de confusión y el detalle están en [`detector/modelos/ficha_modelo.md`](detector/modelos/ficha_modelo.md).

## Límites

- Solo ve el dron cuando su control transmite en 2,4 GHz. Con el DJI Neo 2 en 5,8 GHz, o manejado solo desde el celular, no aparece.
- Los datos de entrenamiento son de un solo día, en un solo cuarto y a 1 m, con una toma larga por clase. Por eso la validación separa minutos y no sesiones.
- El primer modelo usaba también el tráfico WiFi y aprendió que "poco tráfico es dron": de noche daba falsas alarmas. El modelo actual usa solo el espectro. Falta medir falsas alarmas por hora en una noche real con este modelo.

## Organización

```
detector/      captura de datos, entrenamiento, modelo exportado y resultados
firmware/      sketches de Arduino: nodo_detector (S3), devkit_centinela (devkit),
               nodo_captura y pruebas/ de cada pieza
app/           app web del celular y el reentrenamiento en JavaScript
docs/          hardware y conexiones, bitácora de decisiones, diagramas, enunciados
practica-1/    servidor web con FastAPI y PostgreSQL, local y en Azure
practica-2/    broker MQTT, InfluxDB y Grafana en Azure
practica-3/    arquitectura serverless en AWS (IoT Core, Lambda, DynamoDB, S3)
terraform/     infraestructura de las prácticas en Azure y AWS
```

## Para replicarlo

1. Armar el nodo según [`docs/hardware.md`](docs/hardware.md) y probar cada pieza con los sketches de [`firmware/pruebas/`](firmware/pruebas/).
2. Subir [`firmware/nodo_detector`](firmware/nodo_detector/) al S3 y [`firmware/devkit_centinela`](firmware/devkit_centinela/) al devkit. Con eso ya detecta; la app queda en `http://detector.local`.
3. Para grabar datos nuevos desde el PC: [`firmware/nodo_captura`](firmware/nodo_captura/) y el capturador de [`detector/captura-datos/`](detector/captura-datos/).
4. Para reentrenar en el PC: `python rasgos.py` y `python entrenar.py` en [`detector/ml/`](detector/ml/). También se puede desde la app del celular.

Hardware: ESP32-S3 DevKitC-1 (clon N16R8), ESP32 devkit, nRF24L01+ PA+LNA con condensador de 100 µF, módulo ISD1820 con parlante de 8 Ω, y un DJI Neo 2 para las pruebas.

## Prácticas del curso

| Práctica | Qué se pedía | Cómo la resolví |
| --- | --- | --- |
| [1](practica-1/) | Servidor web que reciba datos de un dispositivo IoT, en local y en la nube | FastAPI con PostgreSQL y CSV; en Azure con la base en una subred privada |
| [2](practica-2/) | Broker MQTT, base de series de tiempo y tablero con alertas | Mosquitto con usuarios, InfluxDB en subred privada, Grafana con alertas por Telegram |
| [3](practica-3/) | Arquitectura serverless | AWS IoT Core, DynamoDB, Lambda, API Gateway y un tablero en S3, todo con Terraform |

En las tres el profesor autorizó cambiar el sensor de temperatura del enunciado por los datos del detector. La infraestructura de Azure ya la borré; la de AWS se crea con `terraform apply` cuando hace falta.

## Archivos que no están en el repositorio

Credenciales de WiFi (`secrets.h`), certificados de AWS (`certs.h`, `*.pem`), archivos `.env`, variables y estado de Terraform, y las grabaciones crudas del nRF24 (`detector/captura-datos/raw/`). Cada carpeta que los necesita tiene un `.example` o explica cómo se generan.
