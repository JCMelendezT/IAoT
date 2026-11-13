# Bitácora de decisiones

Lo que fui decidiendo y por qué, de lo más reciente a lo más antiguo.

## Noviembre

**12 nov. App del celular servida por el devkit.** Quería grabar datos y reentrenar sin el PC. El S3 no podía hablar con el celular: su radio y el nRF24 están midiendo los 2,4 GHz, y un enlace BLE o WiFi propio habría metido señal justo en lo que mide. Por eso el que atiende al celular es el devkit, en `http://detector.local`, o en su propia red CENTINELA-RF cuando no hay WiFi. Descarté una app nativa para iPhone porque exige un Mac con Xcode y la cuenta de desarrollador, y Safari no permite Bluetooth desde una página. El devkit guarda cada ventana con sus 35 rasgos (82 bytes, entre 9 y 12 horas de memoria) y desde la app se graba una sesión con su clase o se etiqueta un rato ya grabado. El reentrenamiento corre en el navegador del celular con la misma regresión logística de `entrenar.py`; lo comparé con scikit-learn y da lo mismo. El modelo nuevo viaja al S3 por un segundo cable (GPIO 17 del devkit al GPIO 18 del S3) con un hash para detectar errores, y el S3 lo guarda en su memoria. No usé AWS para esto porque el Learner Lab se apaga a las pocas horas y no sirve para grabar toda una noche.

**12 nov. El modelo deja de mirar el tráfico WiFi.** De noche había falsas alarmas. Al revisar los pesos vi que el modelo había aprendido que "poco tráfico es dron": mientras grababa con el dron el ESP32 vio 37 tramas por ventana, contra 95 en el fondo, y los pesos más fuertes hacia dron eran justo pocas tramas y pocos aparatos. De noche la casa está quieta y pasa lo mismo. Simulando el fondo con 70 % menos tráfico, el 98,7 % de las ventanas salía como dron. Entrené sin esos rasgos: 97,4 % de exactitud, ninguna falsa alarma con la regla de dos ventanas y ninguna con el tráfico de noche simulado. El firmware no cambió porque escoge los rasgos por nombre.

**10 nov. Sistema completo sin pantalla.** La TFT de 1,8" se quedó en blanco con todo lo que probé (ver `hardware.md`), así que la dejé desactivada. El nodo quedó con el sonido del ISD1820 y volvió a mandarle datos al devkit, que los sube a AWS con la clase y la probabilidad que da el modelo.

**10 nov. Primera prueba en la placa.** Alertó apenas prendí el control en 2,4 GHz, con el dron en tierra y volando. Dentro de la casa alertó a 5 m con el dron en tierra y los motores apagados, y a 8 m volando. Afuera, volando, calculo unos 30 m, pero no lo medí.

**10 nov. El modelo corre en el S3.** `rasgos_detector.h` calcula en C++ lo mismo que `rasgos.py` y la alerta se enciende con dos ventanas seguidas de dron. Al comparar el C++ con Python encontré una diferencia: en el PC armaba cada ventana con la hora de llegada al computador, y a veces entraba una fila que el ESP32 había terminado justo después de cerrar la ventana (pasaba en un 6 % de las ventanas). Cambié `rasgos.py` para usar el reloj del ESP32 y desde entonces las 1134 ventanas dan la misma clase en el PC y en la placa.

**10 nov. Primer modelo: regresión logística con tres clases.** Grabé una toma larga por clase en el estudio, a 1 m: fondo, Bluetooth y el dron con su control, en tierra y volando (1134 ventanas de 2 s en total). Tierra y vuelo cuentan como la misma clase. Como no tenía varias sesiones por clase, la validación separa bloques de un minuto, y aparte pruebo con el último 30 % de cada grabación. Es una excepción a la regla de separar por sesión y la cuento como límite. Escogí la regresión logística porque fue de los modelos que mejor dieron y es tan pequeña que no hizo falta cuantizar ni pensar en una red. Bluetooth reemplazó a "celular viendo video" como tercera clase porque el nodo sí lo ve.

## Octubre

**31 oct. Práctica 2 sobre Azure, con la base privada.** Reutilicé la red de la Práctica 1. Mosquitto (con usuarios y ACL), el puente a InfluxDB y Grafana van en la VM pública; InfluxDB en una VM nueva sin IP pública, en su propia subred. El enunciado pedía la base en una subred pública y la dejé privada a propósito. Las alertas de Grafana llegaban por Telegram. Después de sustentar borré toda la infraestructura de Azure.

**21 oct. Práctica 1 funcionando de punta a punta.** El S3 escucha, pasa cada ventana por cable al devkit, el devkit la envía por WiFi y el servidor la guarda en PostgreSQL y en CSV, con una página en vivo. El servidor es FastAPI sobre Ubuntu en WSL con red *mirrored*. Usé HTTP con JSON y dejé MQTT para la Práctica 2.

**21 oct. Dos placas: nodo y pasarela.** El S3 no puede estar en modo promiscuo saltando de canal y conectado al router al mismo tiempo, así que el S3 solo escucha y el devkit se conecta al WiFi. El router de la casa está en el canal 8 (unos 2437 a 2457 MHz), justo dentro de la banda del dron, algo que hubo que tener en cuenta.

**20 oct. El dron se graba con el control en 2,4 GHz.** En DJI Fly fijé Trans. → Frecuencia en 2,4 GHz. Así la actividad entre 2423 y 2456 MHz sube de 4 a 9 veces frente al fondo, con pico cerca de 2445 MHz. En banda dual o en 5,8 GHz el dron desaparece de la cascada. Con el control en 2,4 GHz tampoco aparece ninguna dirección MAC de DJI (su enlace no es WiFi estándar). Manejado solo desde el celular no lo detecto; queda como límite.

**19 oct. Arduino IDE para el firmware.** Lo conozco mejor que ESP-IDF y para este proyecto alcanza. La prueba de vida del nRF24 pasó al segundo intento: el cable de MISO hacía mal contacto.

**19 oct. Azure para la Práctica 1.** El profesor permitió cualquier proveedor y tenía crédito de Azure for Students. La suscripción solo dejaba crear recursos en Chile Central y México Central, y no permitía la talla B1s, así que la VM fue B2ats_v2.

**18 oct. Las prácticas se orientan al detector.** El profesor autorizó reemplazar el sensor de temperatura y humedad por los datos del nodo. Así avanzaba en las prácticas y en el proyecto al mismo tiempo, en un solo repositorio.

**17 oct. Tema del proyecto.** Entre un radar de presencia con WiFi, aprendizaje federado entre ESP32 y un detector de drones, me quedé con el detector: por los ataques con drones que ha habido en Colombia y porque se podía hacer con lo que ya tenía.
