// Reconocedor de gestos: la misma red que entrena detector/gestos/entrenar_gestos.py.
// No usa nada de Arduino, asi se prueba en el PC (prueba_pc/).
//
//   foto 80x60 en gris -> conv 3x3 (8) + ReLU + pool 2x2
//                      -> conv 3x3 (16) + ReLU + pool 2x2
//                      -> conv 3x3 (24) + ReLU -> promedio de cada canal -> 3 salidas -> softmax
//
// Los 4803 numeros de la red no estan en el codigo: se cargan de un archivo (modelo_gestos.bin),
// asi el modelo se cambia por WiFi sin recompilar.
#pragma once
#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#ifndef GESTOS_ASIGNAR
#define GESTOS_ASIGNAR(n) malloc(n)     // en la placa se usa ps_malloc (memoria PSRAM)
#endif

namespace gestos {

enum { ANCHO = 80, ALTO = 60, CLASES = 3, C1 = 8, C2 = 16, C3 = 24 };
enum {
  N_PARAM = (C1 * 9 * 1 + C1) + (C2 * 9 * C1 + C2) + (C3 * 9 * C2 + C3) + (CLASES * C3 + CLASES)
};
static const char* const NOMBRES[CLASES] = {"nada", "abierta", "puno"};

struct Red {
  bool listo = false;
  float media = 0, desv = 1;
  uint32_t suma = 0;                 // suma de comprobacion del archivo (para mostrarla)
  float *p = nullptr, *ent = nullptr, *b1 = nullptr, *b2 = nullptr, *b3 = nullptr, *b4 = nullptr, *b5 = nullptr;
};

inline uint32_t fnv1a(const uint8_t* d, size_t n) {
  uint32_t h = 2166136261u;
  for (size_t i = 0; i < n; i++) h = (h ^ d[i]) * 16777619u;
  return h;
}

inline void liberar(Red& r) {
  free(r.p); free(r.ent); free(r.b1); free(r.b2); free(r.b3); free(r.b4); free(r.b5);
  r = Red();
}

// Devuelve nullptr si todo salio bien, o el motivo del error.
inline const char* cargar(Red& r, const uint8_t* d, size_t n) {
  const size_t CAB = 24, CUERPO = (size_t)N_PARAM * 4;
  if (n != CAB + CUERPO + 4) return "tamano distinto al esperado";
  if (memcmp(d, "GEST", 4)) return "no es un modelo de gestos";
  uint16_t version, w, h; uint8_t nc; float media, desv; uint32_t np, suma;
  memcpy(&version, d + 4, 2); memcpy(&w, d + 6, 2); memcpy(&h, d + 8, 2); nc = d[10];
  memcpy(&media, d + 12, 4); memcpy(&desv, d + 16, 4); memcpy(&np, d + 20, 4);
  if (version != 1) return "version de modelo desconocida";
  if (w != ANCHO || h != ALTO || nc != CLASES) return "la red no tiene la forma esperada";
  if (np != N_PARAM) return "numero de parametros distinto";
  if (!(desv > 1e-6f)) return "desviacion invalida";
  memcpy(&suma, d + CAB + CUERPO, 4);
  if (fnv1a(d + CAB, CUERPO) != suma) return "archivo danado (suma de comprobacion)";

  liberar(r);
  r.p   = (float*)GESTOS_ASIGNAR(CUERPO);
  r.ent = (float*)GESTOS_ASIGNAR(sizeof(float) * ANCHO * ALTO);
  r.b1  = (float*)GESTOS_ASIGNAR(sizeof(float) * ANCHO * ALTO * C1);
  r.b2  = (float*)GESTOS_ASIGNAR(sizeof(float) * (ANCHO / 2) * (ALTO / 2) * C1);
  r.b3  = (float*)GESTOS_ASIGNAR(sizeof(float) * (ANCHO / 2) * (ALTO / 2) * C2);
  r.b4  = (float*)GESTOS_ASIGNAR(sizeof(float) * (ANCHO / 4) * (ALTO / 4) * C2);
  r.b5  = (float*)GESTOS_ASIGNAR(sizeof(float) * (ANCHO / 4) * (ALTO / 4) * C3);
  if (!r.p || !r.ent || !r.b1 || !r.b2 || !r.b3 || !r.b4 || !r.b5) { liberar(r); return "sin memoria"; }
  memcpy(r.p, d + CAB, CUERPO);
  r.media = media; r.desv = desv; r.suma = suma; r.listo = true;
  return nullptr;
}

// Convolucion 3x3, relleno con ceros, ReLU. Imagenes en orden alto x ancho x canales.
inline void conv3(const float* in, int H, int W, int Ci, const float* w, const float* b, int Co, float* out) {
  for (int y = 0; y < H; y++)
    for (int x = 0; x < W; x++)
      for (int co = 0; co < Co; co++) {
        float s = b[co];
        for (int ky = 0; ky < 3; ky++) {
          int yy = y + ky - 1;
          if (yy < 0 || yy >= H) continue;
          for (int kx = 0; kx < 3; kx++) {
            int xx = x + kx - 1;
            if (xx < 0 || xx >= W) continue;
            const float* px = in + ((size_t)yy * W + xx) * Ci;
            const float* wk = w + (((size_t)co * 3 + ky) * 3 + kx) * Ci;
            for (int ci = 0; ci < Ci; ci++) s += px[ci] * wk[ci];
          }
        }
        out[((size_t)y * W + x) * Co + co] = s > 0 ? s : 0;
      }
}

inline void pool2(const float* in, int H, int W, int C, float* out) {
  int h = H / 2, w = W / 2;
  for (int y = 0; y < h; y++)
    for (int x = 0; x < w; x++)
      for (int c = 0; c < C; c++) {
        float a = in[((size_t)(2 * y) * W + 2 * x) * C + c];
        float b = in[((size_t)(2 * y) * W + 2 * x + 1) * C + c];
        float d = in[((size_t)(2 * y + 1) * W + 2 * x) * C + c];
        float e = in[((size_t)(2 * y + 1) * W + 2 * x + 1) * C + c];
        float m = a > b ? a : b;
        if (d > m) m = d;
        if (e > m) m = e;
        out[((size_t)y * w + x) * C + c] = m;
      }
}

// img: 80x60 bytes en gris. prob: 3 probabilidades (nada, abierta, puno).
inline bool inferir(Red& r, const uint8_t* img, float* prob) {
  if (!r.listo) return false;
  const float* c1w = r.p;            const float* c1b = c1w + C1 * 9 * 1;
  const float* c2w = c1b + C1;       const float* c2b = c2w + C2 * 9 * C1;
  const float* c3w = c2b + C2;       const float* c3b = c3w + C3 * 9 * C2;
  const float* dw  = c3b + C3;       const float* db  = dw + CLASES * C3;

  for (int i = 0; i < ANCHO * ALTO; i++) r.ent[i] = (img[i] / 255.0f - r.media) / r.desv;
  conv3(r.ent, ALTO, ANCHO, 1, c1w, c1b, C1, r.b1);
  pool2(r.b1, ALTO, ANCHO, C1, r.b2);
  conv3(r.b2, ALTO / 2, ANCHO / 2, C1, c2w, c2b, C2, r.b3);
  pool2(r.b3, ALTO / 2, ANCHO / 2, C2, r.b4);
  conv3(r.b4, ALTO / 4, ANCHO / 4, C2, c3w, c3b, C3, r.b5);

  const int HW = (ALTO / 4) * (ANCHO / 4);
  float g[C3];
  for (int c = 0; c < C3; c++) {
    float s = 0;
    for (int i = 0; i < HW; i++) s += r.b5[(size_t)i * C3 + c];
    g[c] = s / HW;
  }
  float z[CLASES], zmax = -1e30f;
  for (int k = 0; k < CLASES; k++) {
    float s = db[k];
    for (int c = 0; c < C3; c++) s += g[c] * dw[k * C3 + c];
    z[k] = s;
    if (s > zmax) zmax = s;
  }
  float suma = 0;
  for (int k = 0; k < CLASES; k++) { z[k] = expf(z[k] - zmax); suma += z[k]; }
  for (int k = 0; k < CLASES; k++) prob[k] = z[k] / suma;
  return true;
}

}  // namespace gestos
