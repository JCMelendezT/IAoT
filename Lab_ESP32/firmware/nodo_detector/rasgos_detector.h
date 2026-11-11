// Calculo de rasgos del detector: lo MISMO que hace proyecto-detector-drones/ml/rasgos.py,
// pero en C++ para el ESP32-S3. No usa nada de Arduino, asi se puede probar en el PC.
//
// Por cada ventana de 2 s se juntan:
//   - el resumen WiFi (9 numeros), igual al de la linea W de nodo_captura
//   - todas las filas del nRF24 de esos 2 s (cada fila = 126 canales con valor 0..8)
// y se calculan 35 rasgos con los mismos nombres que en Python. El modelo usa solo
// los que aparecen en DETECTOR_RASGOS (modelo_detector.h); el mapa se arma al arrancar.
#pragma once
#include <math.h>
#include <stdint.h>
#include <string.h>

#define RD_CANALES        126
#define RD_ANCHO_BANDA    6
#define RD_N_BANDAS       (RD_CANALES / RD_ANCHO_BANDA)   // 21 bandas de 6 MHz
#define RD_BANDA_DRON_INI 23                              // 2423 MHz
#define RD_BANDA_DRON_FIN 57                              // sin incluir: hasta 2456 MHz
#define RD_UMBRAL_ACTIVO  0.5f
#define RD_MIN_FILAS      3                               // menos filas: ventana sin espectro
#define RD_N_TODOS        (9 + RD_N_BANDAS + 5)           // 35

// ---------- Acumulador del espectro de una ventana ----------
struct VentanaEspectro {
  uint32_t suma[RD_CANALES];   // suma de cada canal sobre las filas de la ventana
  uint16_t filas;
  uint8_t  maxBandaDron;       // valor mas alto visto en 2423-2456 MHz
  uint8_t  maximo[RD_CANALES]; // maximo de cada canal (solo para el resumen que va a la nube)
  double   sumaMediaFila;      // para la variacion de una fila a otra
  double   sumaMediaFila2;
};

static inline void espectroLimpiar(VentanaEspectro& e) { memset(&e, 0, sizeof(e)); }

static inline void espectroAgregarFila(VentanaEspectro& e, const uint8_t* fila) {
  uint32_t total = 0;
  for (int c = 0; c < RD_CANALES; c++) {
    e.suma[c] += fila[c];
    total += fila[c];
    if (fila[c] > e.maximo[c]) e.maximo[c] = fila[c];
    if (c >= RD_BANDA_DRON_INI && c < RD_BANDA_DRON_FIN && fila[c] > e.maxBandaDron)
      e.maxBandaDron = fila[c];
  }
  double m = (double)total / RD_CANALES;
  e.sumaMediaFila += m;
  e.sumaMediaFila2 += m * m;
  e.filas++;
}

// ---------- Resumen WiFi de una ventana ----------
struct VentanaWifi {
  int32_t tramas, aparatos, mgmt, datos, tamProm, rssiProm, rssiMax, debiles, fuertes;
};

// Nombres de los 35 rasgos, en el orden en que los llena rasgosCalcular()
static const char* const RD_NOMBRES[RD_N_TODOS] = {
  "tramas", "aparatos", "mgmt", "datos", "tam_prom", "rssi_prom", "rssi_max",
  "tramas_debiles", "aparatos_fuertes",
  "banda_2400", "banda_2406", "banda_2412", "banda_2418", "banda_2424", "banda_2430",
  "banda_2436", "banda_2442", "banda_2448", "banda_2454", "banda_2460", "banda_2466",
  "banda_2472", "banda_2478", "banda_2484", "banda_2490", "banda_2496", "banda_2502",
  "banda_2508", "banda_2514", "banda_2520",
  "esp_media", "esp_banda_dron", "esp_max_banda_dron", "esp_canales_activos", "esp_variacion"
};

// Llena todos[RD_N_TODOS]. Devuelve false si la ventana no tiene filas suficientes.
static inline bool rasgosCalcular(const VentanaWifi& w, const VentanaEspectro& e, float* todos) {
  if (e.filas < RD_MIN_FILAS) return false;
  int k = 0;
  todos[k++] = w.tramas;   todos[k++] = w.aparatos; todos[k++] = w.mgmt;
  todos[k++] = w.datos;    todos[k++] = w.tamProm;  todos[k++] = w.rssiProm;
  todos[k++] = w.rssiMax;  todos[k++] = w.debiles;  todos[k++] = w.fuertes;

  float media[RD_CANALES];
  double total = 0, dron = 0;
  int activos = 0;
  for (int c = 0; c < RD_CANALES; c++) {
    media[c] = (float)e.suma[c] / e.filas;
    total += media[c];
    if (c >= RD_BANDA_DRON_INI && c < RD_BANDA_DRON_FIN) dron += media[c];
    if (media[c] > RD_UMBRAL_ACTIVO) activos++;
  }
  for (int b = 0; b < RD_N_BANDAS; b++) {
    float s = 0;
    for (int j = 0; j < RD_ANCHO_BANDA; j++) s += media[b * RD_ANCHO_BANDA + j];
    todos[k++] = s / RD_ANCHO_BANDA;
  }
  double mm = e.sumaMediaFila / e.filas;
  double var = e.sumaMediaFila2 / e.filas - mm * mm;
  todos[k++] = (float)(total / RD_CANALES);
  todos[k++] = (float)(dron / (RD_BANDA_DRON_FIN - RD_BANDA_DRON_INI));
  todos[k++] = e.maxBandaDron;
  todos[k++] = (float)activos / RD_CANALES;
  todos[k++] = (float)sqrt(var > 0 ? var : 0);
  return true;
}

// mapa[i] = posicion en todos[] del rasgo i que pide el modelo. Devuelve el nombre que
// falte (o NULL si estan todos): si pasa, el modelo y este archivo no coinciden.
static inline const char* rasgosPrepararMapa(const char* const* pedidos, int n, int* mapa) {
  for (int i = 0; i < n; i++) {
    mapa[i] = -1;
    for (int j = 0; j < RD_N_TODOS; j++)
      if (strcmp(pedidos[i], RD_NOMBRES[j]) == 0) { mapa[i] = j; break; }
    if (mapa[i] < 0) return pedidos[i];
  }
  return 0;
}

static inline void rasgosElegir(const float* todos, const int* mapa, int n, float* x) {
  for (int i = 0; i < n; i++) x[i] = todos[mapa[i]];
}
