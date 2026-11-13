// Modelo del detector de drones (regresion logistica, 3 clases).
// Generado por detector/ml/entrenar.py el 2026-11-13. No editar a mano.
// Exactitud: 97.4% (bloques de 1 min) y 99.1% (ultimo 30 % de cada grabacion).
//
// Uso: llena x[] con los rasgos de una ventana de 2 s EN ESTE ORDEN y llama
//      int clase = detectorClasificar(x, prob);   // 0=fondo 1=bluetooth 2=dron
#pragma once
#include <math.h>

#define DETECTOR_N_RASGOS 19
#define DETECTOR_N_CLASES 3

static const char* const DETECTOR_CLASES[DETECTOR_N_CLASES] = {"fondo", "bluetooth", "dron"};

// Orden de los rasgos:
//    0 banda_2400
//    1 banda_2406
//    2 banda_2412
//    3 banda_2418
//    4 banda_2424
//    5 banda_2430
//    6 banda_2436
//    7 banda_2442
//    8 banda_2448
//    9 banda_2454
//   10 banda_2460
//   11 banda_2466
//   12 banda_2472
//   13 banda_2478
//   14 esp_media
//   15 esp_banda_dron
//   16 esp_max_banda_dron
//   17 esp_canales_activos
//   18 esp_variacion
static const char* const DETECTOR_RASGOS[DETECTOR_N_RASGOS] = {
  "banda_2400", "banda_2406", "banda_2412", "banda_2418", "banda_2424", "banda_2430", "banda_2436", "banda_2442", "banda_2448", "banda_2454", "banda_2460", "banda_2466", "banda_2472", "banda_2478", "esp_media", "esp_banda_dron", "esp_max_banda_dron", "esp_canales_activos", "esp_variacion"
};

static const float DETECTOR_MEDIA[DETECTOR_N_RASGOS] = {0.1979899f, 0.1928727f, 0.8205556f, 1.012924f, 0.6436255f, 0.7967448f, 0.8469546f, 1.047824f, 0.7648287f, 0.5646022f, 0.552721f, 0.07989436f, 0.1091698f, 0.1110898f, 0.3686591f, 0.7937235f, 3.651675f, 0.1781504f, 0.07304753f};
static const float DETECTOR_ESCALA[DETECTOR_N_RASGOS] = {0.1480145f, 0.2175033f, 0.928384f, 0.7940429f, 0.594416f, 1.018404f, 1.05425f, 1.183074f, 1.143063f, 1.025174f, 0.9963565f, 0.1994755f, 0.1413865f, 0.09339758f, 0.3109011f, 0.8684976f, 1.616551f, 0.1212147f, 0.0437652f};
static const float DETECTOR_PESOS[DETECTOR_N_CLASES][DETECTOR_N_RASGOS] = {
  {-1.583395f, -2.076554f, 0.127675f, 0.07538404f, 0.3921808f, -1.186709f, -0.9570474f, -0.1253967f, -0.2839796f, -0.4629335f, 0.1461288f, -1.129593f, -1.249278f, -0.5860126f, -0.5744656f, -0.5319683f, -0.935729f, -0.818536f, 0.2992547f},  // fondo
  {0.9064051f, 1.021587f, -0.1061034f, 0.04852619f, -0.1289521f, 0.4903554f, -1.059524f, -0.07090267f, -0.4454478f, -1.394121f, -0.6848593f, 0.3850261f, 1.338807f, 0.9153497f, -0.4215276f, -0.4518396f, 0.1663069f, -0.7152795f, 0.4405562f},  // bluetooth
  {0.67699f, 1.054967f, -0.02157159f, -0.1239102f, -0.2632287f, 0.6963536f, 2.016571f, 0.1962994f, 0.7294274f, 1.857054f, 0.5387305f, 0.7445673f, -0.08952905f, -0.3293371f, 0.9959933f, 0.9838079f, 0.7694222f, 1.533815f, -0.7398109f},  // dron
};
static const float DETECTOR_SESGO[DETECTOR_N_CLASES] = {-4.582506f, -0.9616126f, 5.544118f};

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
