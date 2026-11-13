// Caja negra del detector: guarda cada ventana que manda el S3 (linea R) en la memoria
// del devkit (LittleFS), para revisarla y etiquetarla desde la app y reentrenar.
//
// Cada registro ocupa 82 bytes (una ventana de 2 s). Se usan dos archivos que se turnan:
// cuando el actual se llena, el anterior se borra y empieza otro. Con la particion
// "No OTA (2MB APP/2MB SPIFFS)" caben unas 12 horas; con la de fabrica (1,5 MB), unas 9.
#pragma once
#include <LittleFS.h>

#define N_RASGOS 35
#define N_RASGOS_WIFI 9           // los primeros 9 se guardan tal cual; el resto x1000

static const char* const NOMBRES_RASGOS[N_RASGOS] = {
  "tramas", "aparatos", "mgmt", "datos", "tam_prom", "rssi_prom", "rssi_max",
  "tramas_debiles", "aparatos_fuertes",
  "banda_2400", "banda_2406", "banda_2412", "banda_2418", "banda_2424", "banda_2430",
  "banda_2436", "banda_2442", "banda_2448", "banda_2454", "banda_2460", "banda_2466",
  "banda_2472", "banda_2478", "banda_2484", "banda_2490", "banda_2496", "banda_2502",
  "banda_2508", "banda_2514", "banda_2520",
  "esp_media", "esp_banda_dron", "esp_max_banda_dron", "esp_canales_activos", "esp_variacion"
};

// Etiquetas: 0 = sin etiqueta, 1 = fondo, 2 = bluetooth, 3 = dron
static const char* const NOMBRES_ETIQUETA[4] = {"", "fondo", "bluetooth", "dron"};

struct __attribute__((packed)) Registro {
  uint32_t epoch;        // segundos UTC (0 si el devkit aun no tenia la hora)
  uint32_t ventana;      // numero de ventana del S3
  uint8_t  clase;        // lo que dijo el modelo: 0 fondo, 1 bluetooth, 2 dron
  uint8_t  pDron;        // probabilidad de dron, 0..255
  uint8_t  alerta;       // 1 si la alerta estaba activa
  uint8_t  etiqueta;     // lo que realmente era (lo pone la app); 0 = sin etiqueta
  int16_t  r[N_RASGOS];
};
static const size_t OFFSET_ETIQUETA = 11;   // posicion de 'etiqueta' dentro del registro

const char* ARCHIVO_ACTUAL   = "/datos_a.bin";
const char* ARCHIVO_ANTERIOR = "/datos_b.bin";
size_t maxBytesArchivo = 600000;
bool   almacenListo = false;

bool almacenIniciar() {
  almacenListo = LittleFS.begin(true);   // la primera vez formatea
  if (!almacenListo) return false;
  maxBytesArchivo = (LittleFS.totalBytes() * 45 / 100) / sizeof(Registro) * sizeof(Registro);
  return true;
}

int16_t escalar(int i, float v) {
  float e = i < N_RASGOS_WIFI ? v : v * 1000.0f;
  if (e > 32767) e = 32767;
  if (e < -32768) e = -32768;
  return (int16_t)lroundf(e);
}

float desescalar(int i, int16_t v) { return i < N_RASGOS_WIFI ? (float)v : v / 1000.0f; }

void almacenGuardar(const Registro& reg) {
  if (!almacenListo) return;
  File f = LittleFS.open(ARCHIVO_ACTUAL, "a");
  if (!f) return;
  bool lleno = f.size() + sizeof(Registro) > maxBytesArchivo;
  if (!lleno) f.write((const uint8_t*)&reg, sizeof(reg));
  f.close();
  if (lleno) {
    LittleFS.remove(ARCHIVO_ANTERIOR);
    LittleFS.rename(ARCHIVO_ACTUAL, ARCHIVO_ANTERIOR);
    File g = LittleFS.open(ARCHIVO_ACTUAL, "a");
    if (g) { g.write((const uint8_t*)&reg, sizeof(reg)); g.close(); }
  }
}

size_t almacenTamano(const char* ruta) {
  if (!LittleFS.exists(ruta)) return 0;
  File f = LittleFS.open(ruta, "r");
  size_t t = f ? f.size() : 0;
  if (f) f.close();
  return t;
}

uint32_t almacenRegistros() {
  return (almacenTamano(ARCHIVO_ANTERIOR) + almacenTamano(ARCHIVO_ACTUAL)) / sizeof(Registro);
}

// Recorre todos los registros, del mas viejo al mas nuevo. La funcion devuelve false para parar.
template <typename F>
void almacenRecorrer(F funcion) {
  const char* rutas[2] = {ARCHIVO_ANTERIOR, ARCHIVO_ACTUAL};
  Registro reg;
  for (int k = 0; k < 2; k++) {
    if (!LittleFS.exists(rutas[k])) continue;
    File f = LittleFS.open(rutas[k], "r");
    if (!f) continue;
    while (f.read((uint8_t*)&reg, sizeof(reg)) == sizeof(reg)) {
      if (!funcion(reg)) { f.close(); return; }
    }
    f.close();
  }
}

// Pone la etiqueta a todos los registros con epoch entre desde y hasta. Devuelve cuantos cambio.
uint32_t almacenEtiquetar(uint32_t desde, uint32_t hasta, uint8_t etiqueta) {
  const char* rutas[2] = {ARCHIVO_ANTERIOR, ARCHIVO_ACTUAL};
  uint32_t cambiados = 0;
  Registro reg;
  for (int k = 0; k < 2; k++) {
    if (!LittleFS.exists(rutas[k])) continue;
    File f = LittleFS.open(rutas[k], "r+");
    if (!f) continue;
    size_t pos = 0;
    while (f.seek(pos) && f.read((uint8_t*)&reg, sizeof(reg)) == sizeof(reg)) {
      if (reg.epoch >= desde && reg.epoch <= hasta && reg.epoch != 0 && reg.etiqueta != etiqueta) {
        f.seek(pos + OFFSET_ETIQUETA);
        f.write(&etiqueta, 1);
        cambiados++;
      }
      pos += sizeof(reg);
    }
    f.close();
  }
  return cambiados;
}

void almacenBorrar() {
  LittleFS.remove(ARCHIVO_ANTERIOR);
  LittleFS.remove(ARCHIVO_ACTUAL);
}
