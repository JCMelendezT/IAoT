# AGENTS.md — Contexto para agentes (OpenCode / Claude)

Lee este archivo completo antes de tocar código. La fuente de verdad del proyecto es este repositorio; el plan por fases está en `PLAN.md`, el índice de estado en `00-indice.md` y la bitácora de decisiones en `proyecto-detector-drones/decisiones.md`.

## Qué es esto

Repositorio de Juan Camilo Meléndez para la materia **Inteligencia Artificial aplicada al Internet de las Cosas (Edge AI)**, Especialización en IA, UAO. Contiene dos entregas que se desarrollan juntas:

1. **Proyecto final: detector de drones por radiofrecuencia con IA en el borde.** Un ESP32-S3 con un sensor WiFi (modo promiscuo) y un nRF24L01+ (barrido de 126 canales en 2,4 GHz) clasifica la huella de un dron comercial con modelos cuantizados a int8 y emite una alerta local. Diseño completo en `Recursos de clase/Detector de drones por radiofrecuencia con IA en el borde.md`.
2. **Práctica 1: servidor web para un proyecto IoT** (enunciado en `Actividades/Actividad 1 2026-2 (1).md`). Se hace orientada al proyecto: ver `practica-1/README.md`.

## Reglas duras

- **No interferir, bloquear ni derribar drones.** El sistema es solo receptor pasivo. Interferir señales de radio es ilegal en Colombia. No escribas código que transmita para perturbar el espectro.
- **Alcance:** solo 2,4 GHz. No prometas detección en 5,8 GHz ni FPV analógico, ni dirección o distancia exacta.
- **Partición entrenamiento/prueba por sesión de captura**, nunca al azar entre ventanas. Un mismo `sesion_id` no puede estar en ambos conjuntos.
- **Nunca versionar secretos:** credenciales WiFi, llaves AWS, tokens. Usa `secrets.h` o `.env` (ya están en `.gitignore`) y deja un `*.example` con valores falsos.
- **No borres ni reescribas capturas crudas** en `proyecto-detector-drones/captura-datos/raw/`. Todo procesamiento escribe en `procesado/`.
- **Pines:** no asumas pines. Cada conexión se documenta primero en `Lab_ESP32/PINOUT.md`.
- Antes de cualquier acción destructiva (borrar, reescribir historial, flashear sobre firmware ajeno), pide confirmación.

## Hardware disponible

ESP32-S3, ESP32 devkit, nRF24L01+ (uno para el detector), módulo LoRa Ra-02, teclados matriciales, módulos de grabación y parlante, acelerómetros, RFID, pantallas RGB, dron DJI Neo 2 (fuente de datos de prueba). El nRF24 necesita condensador de 10–100 µF entre VCC y GND. La Práctica 1 no usa DHT: el profesor autorizó orientarla al proyecto, así que sus datos son los del propio nodo detector.

Prioriza lo que ya hay. No propongas comprar piezas salvo que Juan lo pida.

## Estructura

```
AGENTS.md / README.md / 00-indice.md
Actividades/                 enunciados (Actividad 1)
Recursos de clase/           PDFs de clase (ignorados por git) y el diseño del detector
Lab_ESP32/
  PINOUT.md                  fuente de verdad de conexiones
  firmware/
    nrf24-espectro/          cascada 126 canales por serie hacia el PC
    wifi-sniffer/            rasgos WiFi por ventana de 2 s
    nodo-detector/           firmware final: sniffer + nRF24 + modelos + alerta
practica-1/
  esp32-cliente/             firmware del cliente que envía datos al servidor
  servidor/                  aplicación web (API + CSV + base de datos)
  datos/                     CSV de salida (la base local no se versiona)
  entrega/                   capturas, diagramas, guion de la presentación
proyecto-detector-drones/
  decisiones.md              bitácora fechada
  captura-datos/             sesiones.csv (registro), raw/ (ignorado), procesado/
  ml/                        extracción de rasgos, entrenamiento, cuantización
  modelos/                   modelos exportados y su ficha de métricas
  resultados/                curva distancia-detección, falsas alarmas/hora, latencia
  informe/                   informe final y presentación
```

## Convenciones

- Documentación, comentarios y mensajes de commit en **español**. Identificadores de código en inglés.
- Sesiones de captura: `AAAAMMDD_<clase>-<estado>_<dist>m_<escenario>`. Clases: `fondo`, `celular-video`, `bluetooth`, `dron-app`, `dron-control`. Estado: `tierra`, `vuelo` o `na`. Ejemplo: `20261020_dron-app-vuelo_3m_pasillo`. Cada sesión se registra en `captura-datos/sesiones.csv`.
- Firmware con ESP-IDF. Una carpeta por sketch/proyecto, con su `README.md` corto (qué hace, cómo flashear, pines que usa).
- Commits pequeños y revisables: un cambio lógico por commit.

## Herramientas y cuándo usarlas

| Herramienta | Para qué en este repo |
| --- | --- |
| MCP `serial` | Leer/escribir por puerto serie con el ESP32. Requiere `SERIAL_MCP_PLUGINS` habilitado (pendiente). |
| `idf.py` | Compilar y flashear. Verificar que resuelva en el PATH antes de usarlo (pendiente). |
| MCP `context7` | Documentación actual de ESP-IDF, TensorFlow Lite Micro, FastAPI, etc. |
| MCP `engram` | Memoria de sesiones (bugs, contexto). Las decisiones formales van en `decisiones.md`, no solo en engram. |
| Skills útiles | `esp32-firmware-engineer`, `diagnosing-bugs`, `tdd`, `work-unit-commits`, `scientific-paper`, `academic-writing`. |

La nube de la Práctica 1 es **AWS** (el enunciado habla de IP elástica y grupos de seguridad). El MCP de Azure no aplica salvo que el profesor acepte otra nube.

## Cómo trabajar

1. Lee `00-indice.md` para saber en qué paso va el proyecto.
2. Haz el cambio más pequeño que avance el siguiente paso del plan.
3. Si tomas o propones una decisión de diseño, regístrala en `proyecto-detector-drones/decisiones.md`.
4. Actualiza `00-indice.md` al cerrar un hito.
