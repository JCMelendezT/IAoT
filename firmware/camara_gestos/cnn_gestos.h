// Reconocedor de gestos: la misma red que entrenan detector/gestos/entrenar_gestos.py (3 capas)
// y detector/gestos/entrenar_gestos_torch.py (de 2 a 6 capas, la que se recomienda).
// No usa nada de Arduino, asi se prueba en el PC (prueba_pc/).
//
//   foto 80x60 en gris -> [conv 3x3 + ReLU + submuestreo 2x2] x (capas - 1)
//                      -> conv 3x3 + ReLU -> promedio de cada canal -> 3 salidas -> softmax
//
// Los numeros de la red NO estan en el codigo: se cargan de un archivo (modelo_gestos.bin) que
// tambien dice cuantas capas y canales tiene, asi el modelo se cambia por WiFi sin recompilar.
#pragma once
#pragma GCC optimize("O3")
#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#ifndef GESTOS_ASIGNAR
#define GESTOS_ASIGNAR(n) malloc(n)     // en la placa se usa ps_malloc (memoria PSRAM)
#endif

namespace gestos {

enum { ANCHO = 80, ALTO = 60, CLASES = 3, MAX_CAPAS = 6, MAX_CANALES = 128 };
static const char* const NOMBRES[CLASES] = {"nada", "abierta", "puno"};

struct Red {
  bool listo = false;
  int capas = 0;
  int canales[MAX_CAPAS] = {0};
  float media = 0, desv = 1;
  uint32_t suma = 0;                 // suma de comprobacion del archivo (para mostrarla)
  float *p = nullptr, *ent = nullptr, *a = nullptr, *b = nullptr;
  size_t n_a = 0, n_b = 0;
};

inline uint32_t fnv1a(const uint8_t* d, size_t n) {
  uint32_t h = 2166136261u;
  for (size_t i = 0; i < n; i++) h = (h ^ d[i]) * 16777619u;
  return h;
}

inline void liberar(Red& r) {
  free(r.p); free(r.ent); free(r.a); free(r.b);
  r = Red();
}

// Numero de parametros de una red con esos canales
inline size_t contar_parametros(int capas, const int* ch) {
  size_t n = 0;
  int ci = 1;
  for (int i = 0; i < capas; i++) { n += (size_t)ch[i] * 9 * ci + ch[i]; ci = ch[i]; }
  return n + (size_t)CLASES * ci + CLASES;
}

// Escribe, por ejemplo, "8-16-32-48"
inline void describir(const Red& r, char* s, size_t n) {
  size_t k = 0;
  s[0] = 0;
  for (int i = 0; i < r.capas && k + 6 < n; i++) k += snprintf(s + k, n - k, i ? "-%d" : "%d", r.canales[i]);
}

// Devuelve nullptr si todo salio bien, o el motivo del error.
//   version 1: cabecera de 24 bytes, red fija de 3 capas (8, 16, 24 canales)
//   version 2: cabecera de 24 bytes + 8 bytes con la cantidad de canales de cada capa
inline const char* cargar(Red& r, const uint8_t* d, size_t n) {
  if (n < 32) return "archivo demasiado corto";
  if (memcmp(d, "GEST", 4)) return "no es un modelo de gestos";
  uint16_t version, w, h; uint8_t nc, capas; float media, desv; uint32_t np, suma;
  memcpy(&version, d + 4, 2); memcpy(&w, d + 6, 2); memcpy(&h, d + 8, 2); nc = d[10]; capas = d[11];
  memcpy(&media, d + 12, 4); memcpy(&desv, d + 16, 4); memcpy(&np, d + 20, 4);
  if (version != 1 && version != 2) return "version de modelo desconocida";
  if (w != ANCHO || h != ALTO || nc != CLASES) return "la red no tiene la forma esperada";
  if (!(desv > 1e-6f)) return "desviacion invalida";

  int ch[MAX_CAPAS] = {0};
  size_t cab = 24;
  if (version == 1) {
    capas = 3; ch[0] = 8; ch[1] = 16; ch[2] = 24;
  } else {
    cab = 32;
    if (capas < 2 || capas > MAX_CAPAS) return "cantidad de capas no valida";
    for (int i = 0; i < capas; i++) {
      ch[i] = d[24 + i];
      if (ch[i] < 1 || ch[i] > MAX_CANALES) return "cantidad de canales no valida";
    }
  }
  if (np != contar_parametros(capas, ch)) return "numero de parametros distinto";
  const size_t cuerpo = (size_t)np * 4;
  if (n != cab + cuerpo + 4) return "tamano distinto al esperado";
  memcpy(&suma, d + cab + cuerpo, 4);
  if (fnv1a(d + cab, cuerpo) != suma) return "archivo danado (suma de comprobacion)";

  // tamanos de los dos buffers de trabajo (salida de la convolucion y salida del submuestreo)
  size_t na = 0, nb = 0;
  int H = ALTO, W2 = ANCHO;
  for (int i = 0; i < capas; i++) {
    size_t conv = (size_t)H * W2 * ch[i];
    if (conv > na) na = conv;
    if (i < capas - 1) {
      H /= 2; W2 /= 2;
      if (H < 1 || W2 < 1) return "demasiadas capas";
      size_t pool = (size_t)H * W2 * ch[i];
      if (pool > nb) nb = pool;
    }
  }

  liberar(r);
  r.p   = (float*)GESTOS_ASIGNAR(cuerpo);
  r.ent = (float*)GESTOS_ASIGNAR(sizeof(float) * ANCHO * ALTO);
  r.a   = (float*)GESTOS_ASIGNAR(sizeof(float) * na);
  r.b   = (float*)GESTOS_ASIGNAR(sizeof(float) * nb);
  if (!r.p || !r.ent || !r.a || !r.b) { liberar(r); return "sin memoria"; }
  memcpy(r.p, d + cab, cuerpo);
  r.capas = capas;
  for (int i = 0; i < capas; i++) r.canales[i] = ch[i];
  r.n_a = na; r.n_b = nb;
  r.media = media; r.desv = desv; r.suma = suma; r.listo = true;
  return nullptr;
}

// Convolucion 3x3, relleno con ceros, ReLU. Imagenes en orden alto x ancho x canales.
// Pesos: [salida][fila][columna][entrada].
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

// Maximo de cada cuadro de 2x2 (si el alto o el ancho es impar, la ultima fila o columna se descarta)
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
  for (int i = 0; i < ANCHO * ALTO; i++) r.ent[i] = (img[i] / 255.0f - r.media) / r.desv;

  const float* p = r.p;
  const float* x = r.ent;
  int H = ALTO, W = ANCHO, Ci = 1;
  for (int i = 0; i < r.capas; i++) {
    const int Co = r.canales[i];
    const float* w = p;  p += (size_t)Co * 9 * Ci;
    const float* b = p;  p += Co;
    conv3(x, H, W, Ci, w, b, Co, r.a);
    if (i < r.capas - 1) {
      pool2(r.a, H, W, Co, r.b);
      H /= 2; W /= 2;
      x = r.b;
    }
    Ci = Co;
  }
  const float* dw = p;
  const float* db = p + (size_t)CLASES * Ci;

  const int HW = H * W;
  float g[MAX_CANALES];
  for (int c = 0; c < Ci; c++) {
    float s = 0;
    for (int i = 0; i < HW; i++) s += r.a[(size_t)i * Ci + c];
    g[c] = s / HW;
  }
  float z[CLASES], zmax = -1e30f;
  for (int k = 0; k < CLASES; k++) {
    float s = db[k];
    for (int c = 0; c < Ci; c++) s += g[c] * dw[k * Ci + c];
    z[k] = s;
    if (s > zmax) zmax = s;
  }
  float suma = 0;
  for (int k = 0; k < CLASES; k++) { z[k] = expf(z[k] - zmax); suma += z[k]; }
  for (int k = 0; k < CLASES; k++) prob[k] = z[k] / suma;
  return true;
}

}  // namespace gestos
