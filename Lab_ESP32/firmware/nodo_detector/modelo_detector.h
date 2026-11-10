// Modelo del detector de drones (regresion logistica, 3 clases).
// Generado por proyecto-detector-drones/ml/entrenar.py el 2026-11-10. No editar a mano.
// Exactitud: 99.3% (bloques de 1 min) y 100.0% (ultimo 30 % de cada grabacion).
//
// Uso: llena x[] con los rasgos de una ventana de 2 s EN ESTE ORDEN y llama
//      int clase = detectorClasificar(x, prob);   // 0=fondo 1=bluetooth 2=dron
#pragma once
#include <math.h>

#define DETECTOR_N_RASGOS 28
#define DETECTOR_N_CLASES 3

static const char* const DETECTOR_CLASES[DETECTOR_N_CLASES] = {"fondo", "bluetooth", "dron"};

// Orden de los rasgos:
//    0 tramas
//    1 aparatos
//    2 mgmt
//    3 datos
//    4 tam_prom
//    5 rssi_prom
//    6 rssi_max
//    7 tramas_debiles
//    8 aparatos_fuertes
//    9 banda_2400
//   10 banda_2406
//   11 banda_2412
//   12 banda_2418
//   13 banda_2424
//   14 banda_2430
//   15 banda_2436
//   16 banda_2442
//   17 banda_2448
//   18 banda_2454
//   19 banda_2460
//   20 banda_2466
//   21 banda_2472
//   22 banda_2478
//   23 esp_media
//   24 esp_banda_dron
//   25 esp_max_banda_dron
//   26 esp_canales_activos
//   27 esp_variacion
static const char* const DETECTOR_RASGOS[DETECTOR_N_RASGOS] = {
  "tramas", "aparatos", "mgmt", "datos", "tam_prom", "rssi_prom", "rssi_max", "tramas_debiles", "aparatos_fuertes", "banda_2400", "banda_2406", "banda_2412", "banda_2418", "banda_2424", "banda_2430", "banda_2436", "banda_2442", "banda_2448", "banda_2454", "banda_2460", "banda_2466", "banda_2472", "banda_2478", "esp_media", "esp_banda_dron", "esp_max_banda_dron", "esp_canales_activos", "esp_variacion"
};

static const float DETECTOR_MEDIA[DETECTOR_N_RASGOS] = {65.55291f, 34.51235f, 56.23986f, 9.313051f, 296.5265f, -67.5776f, -47.86596f, 23.39859f, 4.449735f, 0.1979899f, 0.1928727f, 0.8205556f, 1.012924f, 0.6436255f, 0.7967448f, 0.8469546f, 1.047824f, 0.7648287f, 0.5646022f, 0.552721f, 0.07989436f, 0.1091698f, 0.1110898f, 0.3686591f, 0.7937235f, 3.651675f, 0.1781504f, 0.07304753f};
static const float DETECTOR_ESCALA[DETECTOR_N_RASGOS] = {29.88503f, 13.584f, 26.66317f, 8.638531f, 34.16586f, 3.721413f, 3.787758f, 15.39457f, 1.342112f, 0.1480145f, 0.2175033f, 0.928384f, 0.7940429f, 0.594416f, 1.018404f, 1.05425f, 1.183074f, 1.143063f, 1.025174f, 0.9963565f, 0.1994755f, 0.1413865f, 0.09339758f, 0.3109011f, 0.8684976f, 1.616551f, 0.1212147f, 0.0437652f};
static const float DETECTOR_PESOS[DETECTOR_N_CLASES][DETECTOR_N_RASGOS] = {
  {0.8742491f, 0.5118238f, 1.000768f, -0.06444263f, 0.3840119f, -0.7526828f, 0.2613238f, 0.7821502f, 0.1202065f, -1.51988f, -1.872247f, 0.1288443f, 0.1088237f, 0.07151049f, -0.7619423f, -0.4073706f, -0.1993994f, 0.1625881f, 0.02624369f, 0.1908636f, -0.9370545f, -1.326479f, -0.5749433f, -0.2833349f, -0.2300079f, -0.5795302f, -0.5051515f, -0.06702661f},  // fondo
  {0.1112353f, 0.7877509f, 0.1104013f, 0.04406088f, -0.01260994f, -0.1686209f, -0.08153487f, -0.04781867f, -0.3000188f, 0.9242091f, 1.036108f, -0.1748325f, 0.1118467f, -0.08120711f, 0.1253554f, -0.3985436f, -0.3311005f, -0.4290557f, -0.327154f, -0.3708066f, 0.6238447f, 1.349421f, 0.7469365f, -0.1921041f, -0.2916233f, 0.2312508f, -0.3632749f, 0.2813756f},  // bluetooth
  {-0.9854844f, -1.299575f, -1.111169f, 0.02038175f, -0.3714019f, 0.9213037f, -0.179789f, -0.7343315f, 0.1798123f, 0.5956713f, 0.8361389f, 0.0459882f, -0.2206704f, 0.009696614f, 0.6365868f, 0.8059142f, 0.5304999f, 0.2664676f, 0.3009104f, 0.1799431f, 0.3132098f, -0.02294175f, -0.1719932f, 0.475439f, 0.5216311f, 0.3482794f, 0.8684264f, -0.214349f},  // dron
};
static const float DETECTOR_SESGO[DETECTOR_N_CLASES] = {-4.553478f, 0.7258105f, 3.827668f};

// Devuelve la clase mas probable; si prob no es NULL, deja ahi las 3 probabilidades.
static inline int detectorClasificar(const float* x, float* prob) {
  float z[DETECTOR_N_CLASES];
  float maximo = -1e30f;
  for (int c = 0; c < DETECTOR_N_CLASES; c++) {
    float s = DETECTOR_SESGO[c];
    for (int i = 0; i < DETECTOR_N_RASGOS; i++)
      s += DETECTOR_PESOS[c][i] * (x[i] - DETECTOR_MEDIA[i]) / DETECTOR_ESCALA[i];
    z[c] = s;
    if (s > maximo) maximo = s;
  }
  float suma = 0, mejor = -1; int clase = 0;
  for (int c = 0; c < DETECTOR_N_CLASES; c++) {
    z[c] = expf(z[c] - maximo);
    suma += z[c];
  }
  for (int c = 0; c < DETECTOR_N_CLASES; c++) {
    float p = z[c] / suma;
    if (prob) prob[c] = p;
    if (p > mejor) { mejor = p; clase = c; }
  }
  return clase;
}
