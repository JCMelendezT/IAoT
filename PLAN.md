# Plan completo — explicado sin tecnicismos

Este es el mapa de todo lo que vamos a hacer, en orden. Cada fase dice **qué es**, **por qué se hace**, **qué haces tú**, **qué hace OpenCode** y **cómo sabes que terminaste**. Si una palabra suena rara, búscala en el glosario al final.

---

## La idea en una frase

Vamos a construir una cajita barata (un ESP32-S3 con una antena extra) que "escucha" el aire, reconoce cuándo hay un dron cerca y suena una alarma. De paso, esa cajita le manda lo que ve a un servidor web, y eso es la Práctica 1.

## El mapa general

```
Fase 0  Preparar el computador y las herramientas
Fase 1  Hacer que el hardware "hable" (ver las señales en el PC)
Fase 2  Práctica 1, parte 1: servidor web en tu PC
Fase 3  Práctica 1, parte 2: el mismo servidor en Azure + GitHub + presentación
Fase 4  Grabar datos (el dron, el celular, el fondo...)
Fase 5  Entrenar la IA en el PC
Fase 6  Meter la IA dentro del ESP32-S3
Fase 7  Medir qué tan bien funciona
Fase 8  Informe y presentación final
```

Las fases 0 a 3 cierran la Práctica 1. Las fases 4 a 8 cierran el proyecto final. La Práctica 1 no tiene que esperar a que la IA funcione: el servidor puede recibir datos de prueba mientras tanto.

## Quién hace qué

| Quién | Qué hace |
| --- | --- |
| **Tú** | Todo lo físico: conectar cables, volar el dron, grabar sesiones, crear los recursos en Azure, presentar. Y decidir. |
| **OpenCode** (en tu PC) | Escribe el código, compila, "flashea" el ESP32, lee lo que el ESP32 dice por el cable USB, corre el entrenamiento. |
| **Claude** (este proyecto) | Planear, explicar, revisar decisiones, escribir el informe y la presentación. |

Regla de oro: **cada vez que termines una fase, me cuentas y actualizamos `00-indice.md`.**

---

## Fase 0 — Preparar el computador

**Qué es:** instalar los programas que se necesitan para hablarle al ESP32 y para correr el servidor.

**Por qué:** sin esto no se puede ni empezar. Es como tener la cocina antes de cocinar.

**Qué instalar (en tu Windows):**

1. **ESP-IDF** — el "kit oficial" de Espressif para programar el ESP32. Lo más fácil es instalarlo con la extensión de ESP-IDF para VS Code, que trae un asistente.
2. **Python 3.11 o superior** — para el servidor y para entrenar la IA.
3. **WSL2 con Ubuntu** — un Ubuntu dentro de Windows. El enunciado pide que el servidor corra en Ubuntu, y así no necesitas otro computador.
4. **Git** — ya tienes el repositorio creado; solo falta Git para Windows si no lo tienes.
5. **En OpenCode:** habilitar `SERIAL_MCP_PLUGINS` y comprobar que encuentra `idf.py`. Esto le permite a OpenCode flashear y leer el ESP32 por ti.

**Qué haces tú:** instalar y conectar el ESP32-S3 por USB.

**Qué hace OpenCode:** compila y flashea un programa de prueba ("hola mundo") y lee lo que el ESP32 responde.

**Cómo sabes que terminaste:** el ESP32-S3 aparece como un puerto COM en Windows y OpenCode lee el mensaje "hola" que manda.

---

## Fase 1 — Hacer que el hardware "hable"

**Qué es:** conectar los dos "oídos" del detector y ver en el PC lo que escuchan.

**Por qué:** la IA aprende de datos. Si los sensores no dan datos limpios, todo lo demás falla. Esta fase es la base de todo.

### 1a. El nRF24 (el oído que escanea frecuencias)

- El nRF24 es un chip de radio que puede sintonizar 126 "emisoras" (canales) en la banda de 2,4 GHz, una por una.
- Para cada canal solo puede decir **"aquí hay algo fuerte" o "no hay nada"** (sí/no). No mide qué tan fuerte. Entonces, para cada canal, contamos cuántas veces dijo "sí" en varios barridos. Eso da una imagen: la **cascada** (126 canales × 32 barridos en el tiempo).
- **Detalle que importa:** el nRF24 solo dice "sí" con señales bastante fuertes. Por eso el alcance será corto. Está bien: medir hasta dónde llega es parte del resultado.

**Qué haces tú:**
1. Conectar el nRF24 al ESP32-S3 siguiendo `Lab_ESP32/PINOUT.md` (OpenCode te propone los pines y los anotamos ahí).
2. **Soldar o poner el condensador** (10–100 µF) entre VCC y GND del nRF24, pegado al módulo. Sin esto el nRF24 falla de formas raras, y es el error número uno con este chip.

**Qué hace OpenCode:** el firmware en `Lab_ESP32/firmware/nrf24-espectro/` y un script en Python que dibuja la cascada en el PC en vivo.

**Cómo sabes que terminaste:** ves la cascada en el PC y, cuando pones un video en el celular cerca, aparecen franjas nuevas.

### 1b. El sniffer WiFi (el oído que escucha el WiFi)

- El ESP32-S3 se pone en **modo promiscuo**: escucha todos los paquetes WiFi que pasan por el aire, aunque no sean para él.
- Cada 2 segundos resume lo que escuchó: cuántos paquetes, de qué tamaño, cada cuánto llegan, qué tan fuertes (RSSI) y de qué fabricante es el aparato que los manda.

**Qué hace OpenCode:** el firmware en `Lab_ESP32/firmware/wifi-sniffer/`.

**Cómo sabes que terminaste:** cada 2 segundos sale una línea con esos números por el puerto serie, y cambian cuando prendes el dron.

### 1c. Verificar el dron (muy importante)

- El detector solo escucha 2,4 GHz. Si el DJI Neo 2 se conecta en 5,8 GHz, el detector **no lo va a ver**.
- **Qué haces tú:** prender el Neo 2 conectado al celular y luego al control, y mirar si aparece en la cascada y en el sniffer. Si en la app DJI Fly se puede forzar 2,4 GHz, lo anotamos. Si no aparece, me cuentas antes de seguir, porque cambia el plan.

---

## Fase 2 — Práctica 1, parte 1: servidor en tu PC

**Qué es:** un programa que corre en tu PC (en Ubuntu) y recibe los datos que le manda el ESP32 por WiFi. Los guarda en un archivo CSV (como un Excel simple) y en una base de datos.

**Por qué:** es lo que pide la Práctica 1, y además le sirve al proyecto: queda un historial de lo que el detector vio.

### Un problema y su solución

El ESP32-S3 no puede hacer dos cosas a la vez con su WiFi: **o escucha todo (modo promiscuo) o está conectado a tu router**. Si salta de canal en canal para escuchar, pierde la conexión.

**Solución propuesta:** usar las dos tarjetas que tienes.

```
ESP32-S3 (escucha y decide)  ──cable──▶  ESP32 devkit (mensajero)  ──WiFi──▶  Servidor
```

- El **S3** solo escucha y detecta.
- Le pasa los resultados al **devkit** por tres cables (TX, RX y tierra).
- El **devkit** está conectado a tu WiFi y manda todo al servidor.

Esto es un diseño real de IoT ("nodo sensor + pasarela") y queda muy bien explicado en la presentación.

### Qué se construye

- **Servidor (FastAPI, en Python)** en `practica-1/servidor/`, con tres "puertas" (endpoints):
  - `/api/ventanas` — los resúmenes cada 2 s.
  - `/api/eventos` — cada vez que se detecta algo.
  - `/api/latido` — "sigo vivo" cada cierto tiempo.
- Cada dato que llega se guarda **en CSV y en PostgreSQL** (la base de datos).
- **Firmware del devkit** en `practica-1/esp32-cliente/`.

**Ojo con WSL2:** por defecto, el Ubuntu de WSL2 queda "escondido" detrás de Windows y el ESP32 no lo encuentra en la red. Se arregla activando el modo de red "mirrored" de WSL2 (necesita Windows 11). Si tu Windows no lo tiene, usamos una máquina virtual con Ubuntu.

**Qué haces tú:** instalar Ubuntu en WSL2, poner el nombre y la clave de tu WiFi en un archivo `secrets.h` (que **nunca** se sube a GitHub), conectar los tres cables entre las tarjetas.

**Qué hace OpenCode:** el servidor, el firmware y las pruebas.

**Cómo sabes que terminaste:** abres el CSV y la base de datos y ves filas nuevas llegando mientras el ESP32 está prendido.

---

## Fase 3 — Práctica 1, parte 2: en la nube (Azure)

**Qué es:** el mismo servidor, pero en un computador de Microsoft en internet, para que funcione desde cualquier lugar.

**Por qué Azure:** el profesor autorizó cualquier proveedor, y tienes créditos de **Azure for Students**. Además, Azure for Students trae **12 meses gratis** de justo lo que necesitamos: una máquina virtual pequeña (B1s, 750 horas al mes) y una base de datos PostgreSQL administrada (Flexible Server B1MS, 750 horas al mes y 32 GB). Si esos servicios gratis siguen activos en tu cuenta, los ~20 dólares casi no se tocan.

**Cómo se arma (en palabras simples):**

```
Internet
   │
   ▼
[ Subred "app" ]   Máquina virtual Ubuntu (B1s) con el servidor + IP pública estática
   │
   ▼  (solo la máquina virtual puede pasar, puerto 5432)
[ Subred "db" ]    PostgreSQL Flexible Server (B1MS), con acceso privado: sin IP pública
```

Todo vive dentro de una **red virtual** (VNet) y de un **grupo de recursos** (una "carpeta" que agrupa todo lo del proyecto).

| El enunciado pide | En Azure se llama |
| --- | --- |
| Subred pública | Subred `app`, donde la máquina virtual tiene IP pública |
| Subred privada | Subred `db`, delegada a PostgreSQL, sin IP pública |
| IP elástica | **IP pública estática** |
| Grupos de seguridad | **NSG** (grupos de seguridad de red) |

- **Subred pública:** la parte que se ve desde internet. Ahí va el servidor.
- **Subred privada:** la parte escondida. Ahí va la base de datos, para que nadie de afuera la toque.
- **IP pública estática:** una dirección fija, para que el ESP32 siempre sepa a dónde mandar.
- **NSG:** las "porterías". Al servidor solo se entra por el puerto de la aplicación (y SSH solo desde tu IP). A la base de datos solo entra la subred `app`.

**Por qué una base administrada y no instalar PostgreSQL en otra máquina:** es lo que Azure regala 12 meses, Microsoft se encarga de las copias de seguridad, y te ahorras configurar una segunda máquina en la subred privada.

**Cuidado con los 20 dólares:**
1. Primero revisa en el portal, en **"Servicios gratuitos"** de tu suscripción, que la VM B1s y PostgreSQL B1MS sigan disponibles. Los servicios gratis duran 12 meses desde que se creó o renovó la cuenta.
2. Crea un **presupuesto con alerta** (Cost Management → Presupuestos), por ejemplo de 5 dólares.
3. La **IP pública estática siempre cobra** (unos pocos dólares al mes), aunque la máquina esté apagada. Es el gasto principal.
4. **Detén** la máquina virtual (que quede "desasignada") y la base de datos cuando no las uses. Ojo: Azure vuelve a prender la base sola a los 7 días de detenida.
5. Después de presentar, **borra el grupo de recursos completo**. Eso elimina todo de un golpe y deja de cobrar.

Lo bueno de Azure for Students: si se acaba el crédito, la suscripción se desactiva en vez de cobrarte a la tarjeta.

**Qué haces tú:** seguir los pasos en el portal de Azure (te los doy uno por uno), y guardar capturas de pantalla para la entrega. La primera vez conviene hacerlo a mano en el portal para entender cada pieza y poder explicarla en la presentación.

**Qué hace OpenCode:** prepara el servidor para que lea la dirección de la base desde un archivo `.env`, un script para instalarlo en la máquina virtual, y puede revisar los recursos con su MCP de Azure.

**Entrega de la práctica:** subir el repositorio a GitHub (privado o compartido con el profesor), y la presentación en `practica-1/entrega/`.

**Cómo sabes que terminaste:** el ESP32 manda datos a la IP pública estática y aparecen en la base de datos PostgreSQL de Azure. Lo muestras en vivo.

---

## Fase 4 — Grabar datos

**Qué es:** grabar muchas veces lo que "escucha" el detector en situaciones distintas, con su etiqueta ("esto es dron", "esto es celular"...).

**Por qué:** la IA aprende viendo ejemplos con su respuesta correcta. Sin buenos ejemplos, no aprende.

**Las 5 clases (situaciones):**

| Clase | Qué es |
| --- | --- |
| `fondo` | El WiFi normal de la casa, sin dron |
| `celular-video` | Un celular viendo video (para que la IA no lo confunda con un dron) |
| `bluetooth` | Audífonos o parlante Bluetooth sonando |
| `dron-app` | Neo 2 conectado al celular |
| `dron-control` | Neo 2 con su control |

**Cómo se graba:**
- Cada clase a **1, 3 y 5 metros**. El dron, en tierra y en vuelo.
- Antes de cada toma, marcas la clase con el **teclado matricial**.
- **Varias sesiones en días distintos.** Cada sesión se anota en `proyecto-detector-drones/captura-datos/sesiones.csv`.
- Nombre de cada sesión: `AAAAMMDD_clase-estado_distancia_escenario`. Ejemplo: `20261025_dron-app-vuelo_3m_pasillo`.

**Por qué varios días:** si todo se graba el mismo día, la IA puede aprender "el ruido de ese día" en vez de "el dron". Grabar en días distintos la obliga a aprender lo que de verdad importa.

**Seguridad con el dron (no negociable):** vuelo en interior, despacio, con protectores de hélice, sin personas ni mascotas cerca.

**Cómo sabes que terminaste:** cada clase tiene al menos 3 sesiones de días distintos, y todas están en `sesiones.csv`.

---

## Fase 5 — Entrenar la IA (en el PC)

**Qué es:** enseñarle a un modelo a reconocer cada clase con los datos de la Fase 4.

**Dos modelos, uno por cada "oído":**
1. **Bosque aleatorio** con los números del sniffer WiFi. Es un modelo sencillo que hace muchas preguntas de sí/no ("¿hay muchos paquetes?", "¿el fabricante es DJI?") y vota. Es la **línea base**: el punto de comparación.
2. **CNN 2D pequeña** con la cascada del nRF24. Es una red neuronal que mira la cascada como si fuera una foto y aprende las "formas" que deja un dron.

**La regla más importante:** se separan los datos **por sesión**. Unas sesiones completas sirven para aprender y otras, que el modelo nunca vio, para examinarlo. Si se mezclan ventanas de la misma sesión en ambos lados, el modelo "se copia en el examen" y los resultados salen inflados.

**Qué hace OpenCode:** los scripts en `proyecto-detector-drones/ml/`.

**Cómo sabes que terminaste:** tienes una tabla con qué tan bien detecta "dron" cada modelo en las sesiones de examen.

---

## Fase 6 — Meter la IA en el ESP32-S3

**Qué es:** pasar los modelos del PC al chip, para que decida solo, sin internet.

- **Cuantizar a int8:** "comprimir" el modelo para que use números más pequeños. Pierde muy poca precisión y cabe en el chip.
- La CNN corre con **TensorFlow Lite Micro**. El bosque aleatorio se convierte a código C.
- **Combinar los dos oídos:** se juntan las probabilidades de ambos modelos.
- **Anti-falsas alarmas:** la alarma solo suena si sale "dron" **dos ventanas seguidas**.
- Al detectar: alarma en el parlante, aviso en la pantalla RGB, y el evento viaja al servidor (por la pasarela de la Fase 2).

**Cómo sabes que terminaste:** prendes el dron y, sin PC conectado, la cajita suena.

---

## Fase 7 — Medir qué tan bien funciona

**Qué se mide (y qué significa):**

| Medida | En palabras simples |
| --- | --- |
| Precisión de "dron" | De las veces que dijo "dron", ¿cuántas era de verdad un dron? |
| Exhaustividad de "dron" | De los drones que hubo de verdad, ¿cuántos detectó? |
| Falsas alarmas por hora | ¿Cuántas veces sonó sin dron? (el servidor de la Práctica 1 ayuda a contarlas) |
| Latencia | Desde que prende el dron hasta que suena la alarma, ¿cuántos segundos? |
| Tamaño del modelo | ¿Cuántos KB ocupa en el chip? |
| **Curva de distancia** | A 2, 5, 10, 20 y 30 m, en cuarto, pasillo y patio: ¿hasta dónde detecta? |

La curva de distancia es **el resultado propio del proyecto**. Sirve para decir con honestidad hasta dónde llega.

---

## Fase 8 — Informe y presentación

- Informe en `proyecto-detector-drones/informe/`: problema, diseño, datos, modelos, resultados, límites.
- Presentación con un demo en vivo (prender el dron y que suene).
- Claude ayuda a redactar. OpenCode genera las gráficas desde los resultados.

---

## Riesgos y qué hacemos si pasan

| Si pasa esto... | Hacemos esto |
| --- | --- |
| El nRF24 no responde o da basura | Revisar condensador, cables cortos, alimentación de 3,3 V (nunca 5 V). |
| El Neo 2 transmite en 5,8 GHz | Forzar 2,4 GHz si la app lo permite. Si no, lo replanteamos juntos antes de grabar. |
| El ESP32 no encuentra el servidor de WSL2 | Modo "mirrored" de WSL2 o máquina virtual con Ubuntu. |
| Se acaban los créditos de Azure | Presupuesto con alerta, detener VM y base cuando no se usan, borrar el grupo de recursos al terminar. |
| La CNN no cabe en el chip | Hacerla más pequeña, o quedarse con el bosque aleatorio. |

## Pendientes antes de arrancar

- [ ] **Fechas** de entrega de la Práctica 1 y del proyecto final (para poner semanas a cada fase).
- [ ] ¿Siguen activos los **servicios gratis** de Azure for Students (VM B1s y PostgreSQL B1MS)?
- [ ] ¿Tu Windows es **Windows 11**? (para el modo de red de WSL2)
- [ ] ¿La Práctica 1 es **individual o en equipo**?

---

## Glosario

- **ESP32 / ESP32-S3:** computador diminuto con WiFi. El S3 es la versión más potente, con instrucciones especiales para IA.
- **Firmware:** el programa que vive dentro del ESP32.
- **Flashear:** grabar el firmware en el ESP32 por el cable USB.
- **Puerto serie (COM):** el "chat" por cable USB entre el ESP32 y el PC.
- **2,4 GHz:** la banda de radio que usan el WiFi, el Bluetooth y muchos drones.
- **Canal:** una "emisora" dentro de la banda.
- **Modo promiscuo:** el ESP32 escucha todos los paquetes WiFi, no solo los suyos.
- **RSSI:** qué tan fuerte llega una señal.
- **Cascada:** imagen de qué canales están ocupados a lo largo del tiempo.
- **Ventana:** un pedazo de 2 segundos de datos que se analiza de una vez.
- **API / endpoint:** las "puertas" del servidor a las que el ESP32 manda datos.
- **CSV:** archivo de texto con datos en filas y columnas; se abre en Excel.
- **PostgreSQL / base de datos:** donde se guardan los datos de forma ordenada y consultable.
- **WSL2:** Ubuntu funcionando dentro de Windows.
- **Máquina virtual (VM) / PostgreSQL Flexible Server:** en Azure, un computador alquilado y una base de datos ya armada por Microsoft.
- **VNet / grupo de recursos:** la red privada del proyecto en Azure y la "carpeta" que agrupa todos sus recursos.
- **Subred pública / privada:** zonas de la red en la nube; la pública se ve desde internet, la privada no.
- **IP pública estática ("IP elástica" en AWS):** dirección fija en internet.
- **NSG (grupo de seguridad de red):** reglas de quién puede entrar y por qué puerto.
- **Bosque aleatorio:** modelo de IA hecho de muchos árboles de preguntas sí/no que votan.
- **CNN:** red neuronal que aprende patrones en imágenes.
- **Cuantizar / int8:** comprimir el modelo para que quepa y corra rápido en el chip.
- **Edge AI / IA en el borde:** la IA corre en el dispositivo, no en la nube.
