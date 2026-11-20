# camara_gestos

Cámara ESP32-CAM que reconoce tres gestos y le manda la orden al devkit centinela:

| Gesto | Qué hace el devkit |
| --- | --- |
| Mano abierta | Prueba el sonido (`probar_voz`) |
| Puño | Arma o desarma el sistema: enciende o apaga la voz de alerta (`voz`) |
| Nada | No hace nada |

El detector de drones sigue funcionando igual, con o sin cámara. La cámara solo es un control remoto con visión.

**No se probó todavía en la placa real.** El código compila para la AI-Thinker (1,25 MB de 1,9 MB) y la red neuronal da el mismo resultado en C++ que en Python (diferencia de 1e-7), pero la cámara, el WiFi y el cambio de programa por WiFi falta verlos funcionando.

## Cómo está pensado: cable una sola vez

La tarjeta no trae el adaptador USB. Se programa **una vez** con el devkit como adaptador. Desde ahí todo se hace por WiFi, sin volver a conectar nada:

| Qué cambia | Cómo se hace | Cable |
| --- | --- | --- |
| El programa de la cámara | Subir el `.bin` en `http://camara.local` (sección *Cambiar el programa por WiFi*) | No |
| Los gestos (el modelo) | Subir `modelo_gestos.bin` en la misma página | No |
| El WiFi o la dirección del devkit | La misma página | No |
| Las fotos para entrenar | Se sacan y bajan desde la misma página | No |

## Primera carga con cable (solo una vez)

1. Con el **devkit desconectado del USB**, conecta la ESP32-CAM al devkit así. El devkit solo hace de adaptador, así que su EN va a tierra para que su procesador quede apagado:

   | Devkit | ESP32-CAM |
   | --- | --- |
   | EN | GND |
   | TX0 (GPIO1) | U0T (GPIO1) |
   | RX0 (GPIO3) | U0R (GPIO3) |
   | 5V (VIN) | 5V |
   | GND | GND |
   | (cable aparte) | IO0 a GND |

   Van **derechos, TX con TX y RX con RX** (no cruzados): los nombres del devkit están puestos desde el punto de vista de su procesador, que aquí está apagado, así que el chip USB queda "al revés".

   `IO0` a `GND` en la cámara pone la placa en modo de carga. Usa un cable USB bueno: si la cámara se reinicia sola al intentar, casi siempre es poca corriente. Un condensador de 100 a 470 µF entre 5V y GND ayuda.
2. En Arduino IDE: placa **AI Thinker ESP32-CAM**, y en *Herramientas, Partition Scheme* elige **Minimal SPIFFS (1.9MB APP with OTA/190KB SPIFFS)**. Este paso es obligatorio: con el esquema normal el cambio por WiFi no funciona y habría que volver al cable.
3. Copia `secrets.h.example` como `secrets.h` y pon tu WiFi (también puedes dejarlo para después y agregarlo desde la página), y **cambia `OTA_CLAVE`**.
4. Conecta el devkit al USB del PC, elige su puerto y pulsa *Subir*. Cuando diga `Connecting...`, pulsa el botón **RST** de la cámara.
5. Al terminar: **quita el cable de IO0 a GND** y pulsa RST. Abre el monitor serie a 115200: aparece la IP y `http://camara.local`.
6. Desconecta el devkit del USB, quita los cables de la cámara y devuelve el devkit a su lugar. Ya no los necesitas. La cámara se alimenta con 5V de cualquier cargador.

Si no hay WiFi conocido, la cámara crea su propia red `CAMARA-RF` (clave `camara2026`) y la página queda en `http://192.168.4.1`.

## Sacar fotos, entrenar y subir el modelo

1. Abre `http://camara.local` en el PC (Chrome o Edge). Arriba ves lo que ve la cámara.
2. Usa **Sesión guiada**: pulsas un botón y la página te dice en voz alta qué gesto hacer (mano abierta, puño, nada), cuenta con pitidos y baja sola un `.zip` por tanda. Con 3 rondas son unos 4 minutos. La cámara graba cada tanda por dentro y la página la baja al final, así que si la página se pone lenta no se pierden fotos. Guarda los `.zip` en [`detector/gestos/fotos/`](../../detector/gestos/fotos/). También hay *Sacar fotos a mano* para una sola tanda.
3. Hacen falta **varias tandas por gesto** (mínimo 3 por clase, lo ideal 6): de 150 a 300 fotos por gesto en total, cambiando el fondo, la luz, la distancia y la mano. La clase **Nada** es la que más importa: pon ahí lo que la cámara ve normalmente (la persona sentada, el escritorio, una mano quieta) para que no dispare sin querer.
4. En el PC: `python entrenar_gestos.py` dentro de [`detector/gestos/`](../../detector/gestos/). Dice la exactitud con sesiones que no vio, la matriz de confusión y cuántos disparos falsos por hora tendría.
5. En la página, sección **Subir el modelo**: elige `detector/gestos/modelo/modelo_gestos.bin`. La cámara lo revisa (tamaño y suma de comprobación) y, si está bien, lo usa en el acto y lo guarda. Si está dañado lo rechaza y deja el anterior.

Las fotos de caras, manos y cuartos no se suben a GitHub (`detector/gestos/fotos/` está en `.gitignore`).

## Cambiar el programa por WiFi

1. En Arduino IDE: *Programa, Exportar Binario compilado*.
2. En la carpeta del sketch queda una carpeta `build`. Usa el archivo `camara_gestos.ino.bin`, **no** los que dicen `bootloader`, `partitions` o `merged`.
3. En la página: sección *Cambiar el programa por WiFi*, elige el archivo, escribe la clave (`OTA_CLAVE`) y pulsa *Actualizar*. Tarda unos 20 segundos y la cámara se reinicia sola.

También funciona desde Arduino IDE por red: el puerto aparece como `camara` mientras el PC esté en el mismo WiFi (pide la misma clave).

### Si algo sale mal: modo seguro

Si la cámara falla 3 veces seguidas al arrancar (se cuelga o se reinicia antes de 30 s), entra en **modo seguro**: solo queda el WiFi y la actualización, sin cámara. Así un programa malo no deja la placa inservible: sube uno bueno por la misma página. También sale al pulsar *Reiniciar*.
Cuidado: encender y apagar la cámara varias veces seguidas (por ejemplo al probar cables) cuenta como arranques fallidos.

## Cómo decide

- La cámara toma una foto cada 250 ms (o más lenta si la red tarda: la página muestra los milisegundos de cada inferencia) en gris de 160x120. Se promedia cada 2x2 hasta 80x60 y la red neuronal da tres probabilidades.
- Un gesto vale cuando **5 de las últimas 6 fotos** lo dicen con al menos 80 % de seguridad. Es la misma regla que usa el entrenador para estimar los disparos falsos.
- Después de un gesto no vuelve a disparar hasta que se ve **Nada** otra vez (y hay una pausa de 2,5 s), así tener el puño levantado no arma y desarma sin parar.
- El LED rojo de atrás de la cámara parpadea: 1 vez si la orden salió bien, 2 si el sistema quedó armado, 3 rápidas si no encontró al devkit.

## Cómo le habla al devkit

Por la red, con la API que el devkit ya tiene (el S3 no cambia):

- Mano abierta: `POST /api/ajuste?clave=probar_voz&valor=1`
- Puño: lee `GET /api/estado` (campo `voz`) y manda `POST /api/ajuste?clave=voz&valor=<lo contrario>`

La cámara y el devkit tienen que estar en la **misma red** (el mismo WiFi de la casa o el del celular). Busca al devkit como `detector.local`; si tu red no resuelve nombres, escribe su IP en la página (sección WiFi y devkit) y pulsa *Probar conexión*.

## Seguridad

La página no pide usuario: está pensada para una red de casa o del celular. Solo el cambio de programa pide clave. Detector de drones y cámara no transmiten nada fuera de esa red local.

## Archivos

| Archivo | Para qué |
| --- | --- |
| `camara_gestos.ino` | Cámara, reglas de gestos, WiFi, página y cambio de programa |
| `cnn_gestos.h` | La red neuronal (sin librerías de IA); sus 4803 números vienen del archivo del modelo |
| `pagina.h` | La página web (HTML y JavaScript) |
| `secrets.h.example` | Plantilla para WiFi y clave de actualización |
| `prueba_pc/` | Prueba en el PC de que la red da el mismo resultado que Python |
