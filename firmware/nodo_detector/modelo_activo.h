// Modelo que usa el detector y ordenes que llegan del devkit por el cable (GPIO 18).
//
// Al arrancar se carga el modelo guardado en la memoria del S3 (NVS). Si no hay ninguno,
// se usa el de fabrica (modelo_detector.h). La app del celular puede mandar uno nuevo,
// reentrenado con datos propios, sin volver a subir el firmware.
//
// Ordenes (una linea de texto cada una, terminada en salto de linea):
//   M,<version>,<exactitud>,<n>,<n nombres>,<n medias>,<n escalas>,<n pesos fondo>,
//     <n pesos bluetooth>,<n pesos dron>,<sesgo fondo>,<sesgo bt>,<sesgo dron>,<hash>
//     hash = FNV-1a de 32 bits (8 hex) de todo lo que va antes de la ultima coma.
//   X                 vuelve al modelo de fabrica
//   C,<clave>,<valor> ajuste: alerta (ventanas seguidas de dron para alertar), apagar,
//                     voz (0/1), repetir (segundos entre avisos de voz)
//   Q                 pide la ficha del detector
// Respuestas del S3:
//   A,<OK o ERROR>,<detalle>
//   I,<version>,<exactitud>,<n>,<alerta>,<apagar>,<voz>,<repetir>
#pragma once
#include <Preferences.h>
#include <stdlib.h>
#include <math.h>
#include "rasgos_detector.h"
#include "modelo_detector.h"

struct ModeloActivo {
  uint32_t version;        // 0 = de fabrica
  float    exactitud;      // la que informo quien lo entreno (0..1); 0 si no se sabe
  int32_t  n;
  int32_t  mapa[RD_N_TODOS];
  float    media[RD_N_TODOS];
  float    escala[RD_N_TODOS];
  float    pesos[3][RD_N_TODOS];
  float    sesgo[3];
};

ModeloActivo modelo;
bool modeloListo = false;

// Ajustes (se guardan en la memoria)
int ventanasAlerta = 2;    // "dron" seguidas para encender la alerta
int ventanasApagar = 2;    // ventanas sin dron seguidas para apagarla

Preferences memoria;

// ---------------------------------------------------------------- modelo
bool modeloDeFabrica(ModeloActivo& m) {
  memset(&m, 0, sizeof(m));
  int mapa[DETECTOR_N_RASGOS];
  if (rasgosPrepararMapa(DETECTOR_RASGOS, DETECTOR_N_RASGOS, mapa)) return false;
  m.version = 0;
  m.n = DETECTOR_N_RASGOS;
  for (int i = 0; i < m.n; i++) {
    m.mapa[i] = mapa[i];
    m.media[i] = DETECTOR_MEDIA[i];
    m.escala[i] = DETECTOR_ESCALA[i];
    for (int c = 0; c < 3; c++) m.pesos[c][i] = DETECTOR_PESOS[c][i];
  }
  for (int c = 0; c < 3; c++) m.sesgo[c] = DETECTOR_SESGO[c];
  return true;
}

// todos[] = los RD_N_TODOS rasgos de la ventana. Devuelve la clase y deja las 3 probabilidades.
int modeloClasificar(const ModeloActivo& m, const float* todos, float* prob) {
  float z[3], maximo = -1e30f;
  for (int c = 0; c < 3; c++) {
    float s = m.sesgo[c];
    for (int i = 0; i < m.n; i++) s += m.pesos[c][i] * (todos[m.mapa[i]] - m.media[i]) / m.escala[i];
    z[c] = s;
    if (s > maximo) maximo = s;
  }
  float suma = 0;
  for (int c = 0; c < 3; c++) { z[c] = expf(z[c] - maximo); suma += z[c]; }
  int clase = 0;
  for (int c = 0; c < 3; c++) {
    prob[c] = z[c] / suma;
    if (prob[c] > prob[clase]) clase = c;
  }
  return clase;
}

uint32_t fnv1a(const char* s, size_t largo) {
  uint32_t h = 2166136261u;
  for (size_t i = 0; i < largo; i++) { h ^= (uint8_t)s[i]; h *= 16777619u; }
  return h;
}

// Lee una linea "M,..." y, si todo cuadra, deja el modelo en m. Devuelve NULL o el motivo del error.
// Modifica la linea (la parte en pedazos).
const char* modeloDesdeTexto(char* linea, ModeloActivo& m) {
  char* ultimaComa = strrchr(linea, ',');
  if (!ultimaComa) return "linea_vacia";
  uint32_t esperado = strtoul(ultimaComa + 1, NULL, 16);
  if (fnv1a(linea, ultimaComa - linea) != esperado) return "hash_no_coincide";
  *ultimaComa = 0;

  static char* t[8 + 6 * RD_N_TODOS];
  int nt = 0;
  const int maxT = sizeof(t) / sizeof(t[0]);
  for (char* p = strtok(linea, ","); p && nt < maxT; p = strtok(NULL, ",")) t[nt++] = p;
  if (nt < 4 || strcmp(t[0], "M") != 0) return "formato";
  int n = atoi(t[3]);
  if (n < 1 || n > RD_N_TODOS) return "n_fuera_de_rango";
  if (nt != 4 + 6 * n + 3) return "cantidad_de_campos";

  ModeloActivo nuevo;
  memset(&nuevo, 0, sizeof(nuevo));
  nuevo.version = strtoul(t[1], NULL, 10);
  nuevo.exactitud = atof(t[2]);
  nuevo.n = n;
  const char* nombres[RD_N_TODOS];
  for (int i = 0; i < n; i++) nombres[i] = t[4 + i];
  int mapa[RD_N_TODOS];
  if (rasgosPrepararMapa(nombres, n, mapa)) return "rasgo_desconocido";
  int k = 4 + n;
  for (int i = 0; i < n; i++) nuevo.mapa[i] = mapa[i];
  for (int i = 0; i < n; i++) nuevo.media[i] = atof(t[k++]);
  for (int i = 0; i < n; i++) {
    nuevo.escala[i] = atof(t[k++]);
    if (!(nuevo.escala[i] > 0)) return "escala_invalida";
  }
  for (int c = 0; c < 3; c++)
    for (int i = 0; i < n; i++) {
      nuevo.pesos[c][i] = atof(t[k++]);
      if (!isfinite(nuevo.pesos[c][i])) return "peso_invalido";
    }
  for (int c = 0; c < 3; c++) nuevo.sesgo[c] = atof(t[k++]);
  m = nuevo;
  return NULL;
}

// ---------------------------------------------------------------- memoria
void memoriaCargar() {
  memoria.begin("detector", false);
  ModeloActivo guardado;
  bool cargado = memoria.getBytesLength("modelo") == sizeof(ModeloActivo) &&
                 memoria.getBytes("modelo", &guardado, sizeof(guardado)) == sizeof(guardado) &&
                 guardado.n >= 1 && guardado.n <= RD_N_TODOS;
  if (cargado) {
    modelo = guardado;
    modeloListo = true;
  } else {
    modeloListo = modeloDeFabrica(modelo);
  }
  ventanasAlerta = memoria.getInt("alerta", 2);
  ventanasApagar = memoria.getInt("apagar", 2);
#ifdef HAY_VOZ
  vozActiva = memoria.getBool("voz", true);
  vozRepetirMs = (unsigned long)memoria.getInt("repetir", 5) * 1000UL;
#endif
}

// ---------------------------------------------------------------- ordenes del devkit
void responder(Stream& s, bool ok, const char* detalle) {
  s.printf("A,%s,%s\n", ok ? "OK" : "ERROR", detalle);
  Serial.printf("# Orden del devkit: %s %s\n", ok ? "OK" : "ERROR", detalle);
}

void enviarFicha(Stream& s) {
  int voz = 0, repetir = 0;
#ifdef HAY_VOZ
  voz = vozActiva ? 1 : 0;
  repetir = (int)(vozRepetirMs / 1000);
#endif
  s.printf("I,%lu,%.4f,%d,%d,%d,%d,%d\n", (unsigned long)modelo.version, modelo.exactitud, (int)modelo.n,
           ventanasAlerta, ventanasApagar, voz, repetir);
}

void ordenAjuste(Stream& s, char* resto) {
  char* coma = strchr(resto, ',');
  if (!coma) { responder(s, false, "ajuste_sin_valor"); return; }
  *coma = 0;
  const char* clave = resto;
  int valor = atoi(coma + 1);
  char detalle[48];
  snprintf(detalle, sizeof(detalle), "%s=%d", clave, valor);
  if (!strcmp(clave, "alerta") && valor >= 1 && valor <= 10) {
    ventanasAlerta = valor; memoria.putInt("alerta", valor);
  } else if (!strcmp(clave, "apagar") && valor >= 1 && valor <= 30) {
    ventanasApagar = valor; memoria.putInt("apagar", valor);
#ifdef HAY_VOZ
  } else if (!strcmp(clave, "voz") && (valor == 0 || valor == 1)) {
    vozActiva = valor == 1; memoria.putBool("voz", vozActiva);
  } else if (!strcmp(clave, "repetir") && valor >= 2 && valor <= 120) {
    vozRepetirMs = (unsigned long)valor * 1000UL; memoria.putInt("repetir", valor);
  } else if (!strcmp(clave, "probar_voz")) {
    vozReproducir();
#endif
  } else {
    responder(s, false, detalle);
    return;
  }
  responder(s, true, detalle);
  enviarFicha(s);
}

void procesarOrden(Stream& s, char* linea) {
  if (linea[0] == 'M' && linea[1] == ',') {
    ModeloActivo nuevo;
    const char* error = modeloDesdeTexto(linea, nuevo);
    if (error) { responder(s, false, error); return; }
    modelo = nuevo;
    modeloListo = true;
    memoria.putBytes("modelo", &modelo, sizeof(modelo));
    char detalle[40];
    snprintf(detalle, sizeof(detalle), "modelo_v%lu_%d_rasgos", (unsigned long)modelo.version, (int)modelo.n);
    responder(s, true, detalle);
    enviarFicha(s);
  } else if (linea[0] == 'X' && (linea[1] == 0 || linea[1] == ',')) {
    memoria.remove("modelo");
    modeloListo = modeloDeFabrica(modelo);
    responder(s, modeloListo, "modelo_de_fabrica");
    enviarFicha(s);
  } else if (linea[0] == 'C' && linea[1] == ',') {
    ordenAjuste(s, linea + 2);
  } else if (linea[0] == 'Q') {
    enviarFicha(s);
  } else {
    responder(s, false, "orden_desconocida");
  }
}

// Llamar en cada vuelta del loop: junta lo que llega por el cable y procesa cada linea
void leerOrdenes(HardwareSerial& s) {
  static char buf[4096];
  static int largo = 0;
  while (s.available()) {
    char c = s.read();
    if (c == '\r') continue;
    if (c == '\n') {
      buf[largo] = 0;
      if (largo > 0) procesarOrden(s, buf);
      largo = 0;
    } else if (largo < (int)sizeof(buf) - 1) {
      buf[largo++] = c;
    } else {
      largo = 0;   // linea demasiado larga: se descarta
    }
  }
}
