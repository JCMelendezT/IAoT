# ¿Detector de drones por radiofrecuencia con IA en el borde

Oct 17, 2026 · @Juan M

## Problema y objetivo

En Colombia los ataques con drones crecieron rápido: según UNMAS, pasaron de 61 en 2024 a 333 en 2025, y las Fuerzas Militares reportaron 418 ataques entre enero y septiembre de 2026. Los soldados tienen segundos para saber si un dron vigila o lleva explosivos.

El objetivo es demostrar que un detector barato, con dos sensores de radiofrecuencia y un modelo de IA que corre en una ESP32-S3, puede reconocer la huella de un dron comercial en 2,4 GHz y emitir una alerta, sin enviar datos a la nube.

## Alcance y límites

El sistema detecta drones comerciales que emiten en 2,4 GHz, mediante la huella de su tráfico WiFi y de su enlace en el espectro. Tiene alcance de corto rango, estimado en decenas de metros a campo abierto, a verificar con la prueba de distancia.

No hace y no debe hacer lo siguiente:

- No interfiere, bloquea ni derriba drones. Interferir señales de radio sin permiso es ilegal en Colombia, y tumbar un dron puede causar daños en tierra.
- No detecta drones que vuelen en 5,8 GHz o con enlaces de otra banda, como el video FPV analógico.
- No da dirección ni distancia exacta: solo indica que hay un dron cerca.

## Hardware y materiales

**Disponible:**

| Pieza | Uso |
| --- | --- |
| ESP32-S3 | Cerebro: sniffer WiFi, control del nRF24, modelo, pantalla y alarma |
| nRF24L01+ (uno) | Escáner de 126 canales en 2,4 GHz |
| Pantalla RGB | Cascada del espectro y estado de alerta |
| Parlante | Alarma sonora |
| Teclado matricial | Etiquetar las clases al grabar |
| ESP32 devkit | Opcional: notificación a Telegram o MQTT |
| DJI Neo 2 | Fuente de datos, el dron de prueba |
| Celular y PC | Video de referencia, captura, entrenamiento |

**Por comprar o preparar:** condensador de 10 a 100 µF para el nRF24, cables Dupont y protoboard, cable USB de datos, protectores de hélice para el Neo 2 y cinta métrica.

## Arquitectura

Dos sensores producen rasgos distintos, cada uno pasa por su modelo y las probabilidades se combinan. La alerta solo se activa si la clase "dron" aparece en dos ventanas seguidas, para reducir falsas alarmas.

El sensor WiFi (ESP32-S3 en modo promiscuo) cuenta tramas por ventana de 2 s y extrae tasa, tamaños, tiempos entre tramas, RSSI y el fabricante de la MAC. El sensor de espectro (nRF24L01+) barre 126 canales y arma una cascada de 126 × 32.

## Captura de datos

Las clases son: fondo (WiFi normal sin dron), celular con video, Bluetooth, dron por app (Neo 2 conectado al celular) y dron por control.

Cada clase se graba a 1, 3 y 5 m del nodo, con el dron en tierra y en vuelo. Vuelo en interior, despacio, con protectores de hélice y sin personas cerca. Se toman varias sesiones en días distintos, y el teclado marca la etiqueta antes de cada toma.

## Modelos y despliegue

Se entrena primero una línea base con bosque aleatorio sobre los rasgos WiFi, y después una CNN 2D pequeña sobre la cascada del nRF24. Ambos se cuantizan a int8 con TensorFlow Lite Micro (o Edge Impulse) y corren en la ESP32-S3.

La separación entrenamiento/prueba se hace por sesión, nunca al azar entre ventanas, para no inflar los resultados.

## Evaluación

- Precisión y exhaustividad de la clase "dron".
- Falsas alarmas por hora, que pesan tanto como la detección.
- Latencia desde que el dron enciende hasta la alerta, y tamaño del modelo en KB.
- Curva de detección contra distancia (2, 5, 10, 20 y 30 m) en tres escenarios: cuarto cerrado, pasillo y patio.

La curva de distancia es el resultado propio del proyecto y sirve para decir con honestidad hasta dónde llega el sistema.

## Plan de trabajo y fuentes

1. Conectar el nRF24 con condensador y lograr la primera cascada en el PC.
2. Activar el sniffer WiFi y extraer rasgos por ventana.
3. Grabar las cinco clases en varias sesiones.
4. Entrenar la línea base y la CNN, y cuantizar.
5. Desplegar en la S3 y medir detección, falsas alarmas y alcance.

Fuentes consultadas: [El País (UNMAS, 2026)](https://www.elpais.com.co/judicial/onu-asegura-que-ataques-con-drones-y-explosivos-se-han-disparado-en-colombia-1722.html); [Cambio (Fuerzas Militares, 2026)](https://cambiocolombia.com/conflicto-armado-en-colombia/articulo/2026/9/drones-explosivos-418-ataques-contra-militares-dejan-25-muertos-en-colombia); [Resolución ANE 509 de 2019](https://normograma.mintic.gov.co/mintic/docs/pdf/resolucion_ane_0509_2019.pdf).
