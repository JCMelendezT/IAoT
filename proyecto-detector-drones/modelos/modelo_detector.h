// Modelo del detector de drones (regresion logistica, 3 clases).
// Generado por proyecto-detector-drones/ml/entrenar.py el 2026-11-10. No editar a mano.
// Exactitud: 99.2% (bloques de 1 min) y 100.0% (ultimo 30 % de cada grabacion).
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

static const float DETECTOR_MEDIA[DETECTOR_N_RASGOS] = {65.55291f, 34.51235f, 56.23986f, 9.313051f, 296.5265f, -67.5776f, -47.86596f, 23.39859f, 4.449735f, 0.1982295f, 0.1930009f, 0.8204824f, 1.012438f, 0.6446123f, 0.7960835f, 0.8466967f, 1.047678f, 0.764866f, 0.564723f, 0.5525509f, 0.0800679f, 0.1088791f, 0.1110443f, 0.3686384f, 0.7937591f, 3.657848f, 0.1775135f, 0.07321587f};
static const float DETECTOR_ESCALA[DETECTOR_N_RASGOS] = {29.88503f, 13.584f, 26.66317f, 8.638531f, 34.16586f, 3.721413f, 3.787758f, 15.39457f, 1.342112f, 0.1479466f, 0.2174254f, 0.9276096f, 0.7920769f, 0.5935016f, 1.017771f, 1.054055f, 1.18262f, 1.143418f, 1.024492f, 0.9959048f, 0.2007184f, 0.1409206f, 0.09354738f, 0.3110097f, 0.8686923f, 1.609946f, 0.1215408f, 0.04380179f};
static const float DETECTOR_PESOS[DETECTOR_N_CLASES][DETECTOR_N_RASGOS] = {
  {0.8866088f, 0.4950863f, 1.022618f, -0.08912415f, 0.3933754f, -0.8343831f, 0.2714899f, 0.765725f, 0.09926107f, -1.499141f, -1.855012f, 0.12025f, 0.1196909f, 0.09591947f, -0.7718343f, -0.3867468f, -0.1401125f, 0.1850362f, 0.03974273f, 0.1871031f, -0.939102f, -1.280047f, -0.6259528f, -0.2615751f, -0.1987604f, -0.6419583f, -0.4625274f, -0.1047609f},  // fondo
  {0.09794546f, 0.80163f, 0.09228439f, 0.05400319f, -0.02173835f, -0.1090147f, -0.09418924f, -0.02899851f, -0.2866702f, 0.9308799f, 1.045929f, -0.1639231f, 0.1048163f, -0.1147223f, 0.1202452f, -0.447738f, -0.3640127f, -0.4445107f, -0.3405114f, -0.3729857f, 0.604407f, 1.330128f, 0.7472217f, -0.2147967f, -0.3256048f, 0.3117529f, -0.3586759f, 0.3047177f},  // bluetooth
  {-0.9845543f, -1.296716f, -1.114902f, 0.03512097f, -0.371637f, 0.9433978f, -0.1773006f, -0.7367265f, 0.1874091f, 0.5682615f, 0.8090837f, 0.04367314f, -0.2245072f, 0.01880284f, 0.6515891f, 0.8344848f, 0.5041252f, 0.2594745f, 0.3007687f, 0.1858826f, 0.334695f, -0.05008137f, -0.1212688f, 0.4763718f, 0.5243651f, 0.3302053f, 0.8212033f, -0.1999568f},  // dron
};
static const float DETECTOR_SESGO[DETECTOR_N_CLASES] = {-4.538884f, 0.6844898f, 3.854394f};

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
