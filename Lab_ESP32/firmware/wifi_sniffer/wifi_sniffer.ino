// Sniffer WiFi (modo promiscuo) con ESP32-S3 (Arduino IDE)
// SOLO ESCUCHA: no se conecta a ninguna red ni transmite paquetes.
//
// Que hace, en simple:
//  - El ESP32 escucha TODOS los paquetes WiFi del aire, aunque no sean para el.
//  - Cada 2 segundos (una "ventana") resume quien transmitio: direccion MAC,
//    fabricante, canal, cuantos paquetes, tamano promedio, fuerza de la senal (RSSI)
//    y el nombre de la red (SSID) si lo anuncia.
//  - Marca con "DJI !!!" los aparatos cuyo fabricante registrado es DJI.
//
// Modos de canal (escribe en la cajita del Monitor Serie y dale Enter):
//    0      -> salta por los canales 1 a 13 (una vuelta completa por ventana)
//    1..13  -> se queda fijo en ese canal (mas paquetes y tiempos entre tramas reales)
//
// Lineas que empiezan con "R," son el resumen de la ventana para el PC:
//    R,ventana,canal(0=salto),tramas,dispositivos,tramas_dji,rssi_max_dji

#include <WiFi.h>
#include "esp_wifi.h"
#include "esp_timer.h"

// El nRF24 sigue conectado: lo dejamos quieto
const int PIN_CE  = 9;
const int PIN_CSN = 10;

const int VENTANA_MS    = 2000;
const int SALTO_MS      = 150;   // 13 canales x 150 ms ~ 2 s
const int MAX_DISP      = 64;
const int MOSTRAR_TOP   = 10;

// Prefijos MAC (OUI) registrados por SZ DJI Technology Co., Ltd.
// Fuente: registro IEEE via maclookup.app (consultado 2026-10-20)
const uint8_t OUI_DJI[][3] = {
  {0x04,0xA8,0x5A}, {0x0C,0x9A,0xE6}, {0x34,0xD2,0x62}, {0x48,0x1C,0xB9},
  {0x4C,0x43,0xF6}, {0x58,0xB8,0x58}, {0x60,0x60,0x1F}, {0x88,0x29,0x85},
  {0x8C,0x58,0x23}, {0xE4,0x7A,0x2C}
};

struct Disp {
  bool     usado;
  uint8_t  mac[6];
  char     ssid[33];
  uint8_t  canal;
  uint32_t tramas, bytes, mgmt, datos;
  int32_t  rssiSuma;
  int8_t   rssiMax;
  int64_t  ultimoUs;
  uint64_t gapSuma;
  uint32_t gapN;
};

Disp tabla[MAX_DISP];
static Disp copia[MAX_DISP];
volatile uint32_t tramasVentana = 0;
portMUX_TYPE candado = portMUX_INITIALIZER_UNLOCKED;

bool modoSalto = true;
uint8_t canalActual = 1;
uint32_t numVentana = 0;
unsigned long inicioVentana = 0, ultimoSalto = 0;

bool esDJI(const uint8_t* mac) {
  for (auto& o : OUI_DJI)
    if (mac[0] == o[0] && mac[1] == o[1] && mac[2] == o[2]) return true;
  return false;
}

int buscarOCrear(const uint8_t* mac) {
  int libre = -1;
  for (int i = 0; i < MAX_DISP; i++) {
    if (tabla[i].usado) {
      if (memcmp(tabla[i].mac, mac, 6) == 0) return i;
    } else if (libre < 0) libre = i;
  }
  if (libre >= 0) {
    memset(&tabla[libre], 0, sizeof(Disp));
    tabla[libre].usado = true;
    tabla[libre].rssiMax = -127;
    memcpy(tabla[libre].mac, mac, 6);
  }
  return libre;  // -1 si la tabla esta llena
}

// Se llama por cada paquete que llega. Debe ser rapida.
void alRecibir(void* buf, wifi_promiscuous_pkt_type_t tipo) {
  if (tipo != WIFI_PKT_MGMT && tipo != WIFI_PKT_DATA) return;
  const wifi_promiscuous_pkt_t* p = (const wifi_promiscuous_pkt_t*)buf;
  int largo = p->rx_ctrl.sig_len;
  if (largo < 24) return;
  const uint8_t* f = p->payload;
  const uint8_t* emisor = f + 10;          // direccion 2 = quien transmite
  uint8_t subtipo = (f[0] >> 4) & 0x0F;
  int64_t ahora = esp_timer_get_time();

  portENTER_CRITICAL(&candado);
  tramasVentana++;
  int i = buscarOCrear(emisor);
  if (i >= 0) {
    Disp& d = tabla[i];
    d.tramas++;
    d.bytes += largo;
    d.rssiSuma += p->rx_ctrl.rssi;
    if (p->rx_ctrl.rssi > d.rssiMax) d.rssiMax = p->rx_ctrl.rssi;
    d.canal = p->rx_ctrl.channel;
    if (d.ultimoUs) { d.gapSuma += (ahora - d.ultimoUs); d.gapN++; }
    d.ultimoUs = ahora;
    if (tipo == WIFI_PKT_MGMT) d.mgmt++; else d.datos++;
    // Beacon (8) o respuesta de sondeo (5): leer el nombre de la red
    if (tipo == WIFI_PKT_MGMT && (subtipo == 8 || subtipo == 5) && largo > 38 && f[36] == 0) {
      uint8_t n = f[37];
      if (n > 32) n = 32;
      if (38 + n <= largo) {
        for (int k = 0; k < n; k++) {
          char c = (char)f[38 + k];
          d.ssid[k] = (c >= 32 && c <= 126) ? c : '?';
        }
        d.ssid[n] = 0;
      }
    }
  }
  portEXIT_CRITICAL(&candado);
}

void cambiarCanal(uint8_t c) {
  canalActual = c;
  esp_wifi_set_channel(c, WIFI_SECOND_CHAN_NONE);
}

void leerComandos() {
  if (!Serial.available()) return;
  String s = Serial.readString();
  s.trim();
  if (s.length() == 0) return;
  int c = s.toInt();
  if (c == 0) {
    modoSalto = true;
    Serial.println("# Modo: salto por canales 1-13");
  } else if (c >= 1 && c <= 13) {
    modoSalto = false;
    cambiarCanal(c);
    Serial.printf("# Modo: canal fijo %d\n", c);
  } else {
    Serial.println("# Comando no valido. Usa 0 (salto) o 1 a 13 (canal fijo).");
  }
}

void reportar() {
  uint32_t total;
  portENTER_CRITICAL(&candado);
  memcpy(copia, tabla, sizeof(tabla));
  total = tramasVentana;
  tramasVentana = 0;
  for (int i = 0; i < MAX_DISP; i++) {
    if (!tabla[i].usado) continue;
    if (tabla[i].tramas == 0) { tabla[i].usado = false; continue; }  // libera aparatos que ya no aparecen
    tabla[i].tramas = tabla[i].bytes = tabla[i].mgmt = tabla[i].datos = 0;
    tabla[i].rssiSuma = 0; tabla[i].rssiMax = -127;
    tabla[i].gapSuma = 0; tabla[i].gapN = 0; tabla[i].ultimoUs = 0;
  }
  portEXIT_CRITICAL(&candado);

  // Ordenar de mas a menos paquetes (solo los que transmitieron en esta ventana)
  int idx[MAX_DISP], n = 0;
  for (int i = 0; i < MAX_DISP; i++) if (copia[i].usado && copia[i].tramas > 0) idx[n++] = i;
  for (int a = 0; a < n; a++)
    for (int b = a + 1; b < n; b++)
      if (copia[idx[b]].tramas > copia[idx[a]].tramas) { int t = idx[a]; idx[a] = idx[b]; idx[b] = t; }

  uint32_t tramasDJI = 0; int rssiDJI = -127;
  for (int k = 0; k < n; k++) {
    Disp& d = copia[idx[k]];
    if (esDJI(d.mac)) { tramasDJI += d.tramas; if (d.rssiMax > rssiDJI) rssiDJI = d.rssiMax; }
  }

  numVentana++;
  Serial.println();
  if (modoSalto) Serial.printf("=== Ventana %lu | canal: salto 1-13 | tramas: %lu | aparatos: %d ===\n", numVentana, total, n);
  else           Serial.printf("=== Ventana %lu | canal fijo: %d | tramas: %lu | aparatos: %d ===\n", numVentana, canalActual, total, n);
  Serial.println("  MAC                fabricante  can tramas  tam  rssi  max  gap_ms  SSID");
  for (int k = 0; k < n && k < MOSTRAR_TOP; k++) {
    Disp& d = copia[idx[k]];
    const char* fab = esDJI(d.mac) ? "DJI !!!  " : ((d.mac[0] & 0x02) ? "aleatoria" : "-        ");
    int tam = d.bytes / d.tramas;
    int rssi = d.rssiSuma / (int32_t)d.tramas;
    long gap = d.gapN ? (long)(d.gapSuma / d.gapN / 1000) : -1;
    Serial.printf("  %02X:%02X:%02X:%02X:%02X:%02X  %s  %3d %6lu %4d %5d %4d %7ld  %s\n",
      d.mac[0], d.mac[1], d.mac[2], d.mac[3], d.mac[4], d.mac[5],
      fab, d.canal, d.tramas, tam, rssi, d.rssiMax, gap, d.ssid);
  }
  Serial.printf("R,%lu,%d,%lu,%d,%lu,%d\n", numVentana, modoSalto ? 0 : canalActual, total, n, tramasDJI, rssiDJI);
}

void setup() {
  Serial.begin(115200);
  Serial.setTimeout(50);
  delay(2000);
  pinMode(PIN_CE, OUTPUT);  digitalWrite(PIN_CE, LOW);
  pinMode(PIN_CSN, OUTPUT); digitalWrite(PIN_CSN, HIGH);

  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  wifi_promiscuous_filter_t filtro = {};
  filtro.filter_mask = WIFI_PROMIS_FILTER_MASK_MGMT | WIFI_PROMIS_FILTER_MASK_DATA;
  esp_wifi_set_promiscuous_filter(&filtro);
  esp_wifi_set_promiscuous_rx_cb(&alRecibir);
  esp_wifi_set_promiscuous(true);
  cambiarCanal(1);

  Serial.println("# Sniffer WiFi listo. Escribe 0 = salto de canales, 1..13 = canal fijo.");
  inicioVentana = ultimoSalto = millis();
}

void loop() {
  leerComandos();
  unsigned long ahora = millis();
  if (modoSalto && ahora - ultimoSalto >= SALTO_MS) {
    cambiarCanal(canalActual % 13 + 1);
    ultimoSalto = ahora;
  }
  if (ahora - inicioVentana >= VENTANA_MS) {
    reportar();
    inicioVentana = ahora;
  }
  delay(2);
}
