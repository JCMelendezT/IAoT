// Nodo de captura: los DOS "oidos" al mismo tiempo (Arduino IDE, ESP32-S3)
//   1) nRF24L01+  -> cascada del espectro 2,4 GHz (126 canales)
//   2) WiFi del ESP32 en modo promiscuo -> resumen del trafico WiFi cada 2 s
// SOLO ESCUCHA: el nRF24 queda en modo receptor y el WiFi no se conecta ni transmite.
//
// Este programa no se usa con el Monitor Serie: lo lee el programa del PC
// "proyecto-detector-drones/captura-datos/Capturar.bat", que guarda cada sesion con su etiqueta.
//
// Formato de las lineas que envia (una por linea):
//   N,<ms>,<126 digitos 0-8>   -> una fila de la cascada: cuantas veces (de 8 barridos)
//                                 hubo senal fuerte en cada canal, de 2400 a 2525 MHz
//   W,<ms>,<ventana>,<canal>,<tramas>,<aparatos>,<mgmt>,<datos>,<tam_prom>,
//     <rssi_prom>,<rssi_max>,<tramas_debiles>,<aparatos_fuertes>,<tramas_dji>,<rssi_max_dji>
//                              -> resumen WiFi de una ventana de 2 s (canal 0 = salto 1-13)
//   # ...                      -> mensajes y notas, no son datos
//
// Conexiones: CE=9  CSN=10  MOSI=11  SCK=12  MISO=13  VCC=3V3  GND=GND (ver Lab_ESP32/PINOUT.md)

#include <SPI.h>
#include <WiFi.h>
#include "esp_wifi.h"
#include "esp_timer.h"

// ---------- Pines del nRF24 ----------
const int PIN_CE   = 9;
const int PIN_CSN  = 10;
const int PIN_MOSI = 11;
const int PIN_SCK  = 12;
const int PIN_MISO = 13;

// ---------- Ajustes de la cascada ----------
const int CANALES           = 126;
const int BARRIDOS_POR_FILA = 8;
const int ESPERA_US         = 200;

// ---------- Ajustes del sniffer ----------
const int VENTANA_MS        = 2000;
const int SALTO_MS          = 150;   // 13 canales x 150 ms ~ 2 s
const int MAX_DISP          = 64;
const int RSSI_DEBIL        = -75;   // por debajo: senal debil (vecinos lejanos)
const int RSSI_FUERTE       = -55;   // por encima: aparato muy cerca

const uint8_t OUI_DJI[][3] = {       // prefijos MAC registrados por SZ DJI Technology
  {0x04,0xA8,0x5A}, {0x0C,0x9A,0xE6}, {0x34,0xD2,0x62}, {0x48,0x1C,0xB9},
  {0x4C,0x43,0xF6}, {0x58,0xB8,0x58}, {0x60,0x60,0x1F}, {0x88,0x29,0x85},
  {0x8C,0x58,0x23}, {0xE4,0x7A,0x2C}
};

SPISettings ajustesSPI(1000000, MSBFIRST, SPI_MODE0);
SemaphoreHandle_t candadoSerie;

// ======================================================================
//  Utilidades
// ======================================================================
void imprimirLinea(const char* s) {
  xSemaphoreTake(candadoSerie, portMAX_DELAY);
  Serial.println(s);
  xSemaphoreGive(candadoSerie);
}

// ======================================================================
//  Oido 1: nRF24 (corre en su propia tarea)
// ======================================================================
uint8_t leerRegistro(uint8_t reg) {
  SPI.beginTransaction(ajustesSPI);
  digitalWrite(PIN_CSN, LOW);
  SPI.transfer(0x00 | reg);
  uint8_t v = SPI.transfer(0xFF);
  digitalWrite(PIN_CSN, HIGH);
  SPI.endTransaction();
  return v;
}

void escribirRegistro(uint8_t reg, uint8_t valor) {
  SPI.beginTransaction(ajustesSPI);
  digitalWrite(PIN_CSN, LOW);
  SPI.transfer(0x20 | reg);
  SPI.transfer(valor);
  digitalWrite(PIN_CSN, HIGH);
  SPI.endTransaction();
}

bool iniciarNRF() {
  pinMode(PIN_CE, OUTPUT);  digitalWrite(PIN_CE, LOW);
  pinMode(PIN_CSN, OUTPUT); digitalWrite(PIN_CSN, HIGH);
  SPI.begin(PIN_SCK, PIN_MISO, PIN_MOSI);
  delay(100);
  uint8_t original = leerRegistro(0x05);
  escribirRegistro(0x05, 0x4C);
  bool ok = (leerRegistro(0x05) == 0x4C);
  escribirRegistro(0x05, original);
  if (!ok) return false;
  escribirRegistro(0x01, 0x00);  // EN_AA: sin acuses
  escribirRegistro(0x06, 0x07);  // RF_SETUP: 1 Mbps
  escribirRegistro(0x00, 0x03);  // CONFIG: encendido + SOLO RECEPTOR
  delay(5);
  return true;
}

void tareaEspectro(void*) {
  static char linea[24 + CANALES];
  for (;;) {
    uint8_t cuenta[CANALES] = {0};
    for (int b = 0; b < BARRIDOS_POR_FILA; b++) {
      for (int c = 0; c < CANALES; c++) {
        escribirRegistro(0x05, c);
        digitalWrite(PIN_CE, HIGH);
        delayMicroseconds(ESPERA_US);
        digitalWrite(PIN_CE, LOW);
        if (leerRegistro(0x09) & 0x01) cuenta[c]++;
      }
      vTaskDelay(1);  // deja respirar a las demas tareas
    }
    int n = snprintf(linea, 24, "N,%lu,", (unsigned long)millis());
    for (int c = 0; c < CANALES; c++) linea[n + c] = '0' + cuenta[c];
    linea[n + CANALES] = 0;
    imprimirLinea(linea);
  }
}

// ======================================================================
//  Oido 2: sniffer WiFi
// ======================================================================
struct Disp {
  bool     usado;
  uint8_t  mac[6];
  uint32_t tramas;
  int8_t   rssiMax;
};

Disp tabla[MAX_DISP];
portMUX_TYPE candadoWifi = portMUX_INITIALIZER_UNLOCKED;

// Contadores de la ventana actual
uint32_t vTramas = 0, vMgmt = 0, vDatos = 0, vBytes = 0, vDebiles = 0, vDJI = 0;
int32_t  vRssiSuma = 0;
int      vRssiMax = -127, vRssiMaxDJI = -127;

bool modoSalto = true;
uint8_t canalActual = 1;
uint32_t numVentana = 0;
unsigned long inicioVentana = 0, ultimoSalto = 0;

bool esDJI(const uint8_t* mac) {
  for (auto& o : OUI_DJI)
    if (mac[0] == o[0] && mac[1] == o[1] && mac[2] == o[2]) return true;
  return false;
}

void alRecibir(void* buf, wifi_promiscuous_pkt_type_t tipo) {
  if (tipo != WIFI_PKT_MGMT && tipo != WIFI_PKT_DATA) return;
  const wifi_promiscuous_pkt_t* p = (const wifi_promiscuous_pkt_t*)buf;
  int largo = p->rx_ctrl.sig_len;
  if (largo < 24) return;
  const uint8_t* emisor = p->payload + 10;  // direccion 2 = quien transmite
  int rssi = p->rx_ctrl.rssi;
  bool dji = esDJI(emisor);

  portENTER_CRITICAL(&candadoWifi);
  vTramas++;
  vBytes += largo;
  vRssiSuma += rssi;
  if (rssi > vRssiMax) vRssiMax = rssi;
  if (rssi < RSSI_DEBIL) vDebiles++;
  if (tipo == WIFI_PKT_MGMT) vMgmt++; else vDatos++;
  if (dji) { vDJI++; if (rssi > vRssiMaxDJI) vRssiMaxDJI = rssi; }

  int libre = -1, i = -1;
  for (int k = 0; k < MAX_DISP; k++) {
    if (tabla[k].usado) { if (memcmp(tabla[k].mac, emisor, 6) == 0) { i = k; break; } }
    else if (libre < 0) libre = k;
  }
  if (i < 0 && libre >= 0) {
    i = libre;
    tabla[i].usado = true;
    memcpy(tabla[i].mac, emisor, 6);
    tabla[i].tramas = 0;
    tabla[i].rssiMax = -127;
  }
  if (i >= 0) {
    tabla[i].tramas++;
    if (rssi > tabla[i].rssiMax) tabla[i].rssiMax = rssi;
  }
  portEXIT_CRITICAL(&candadoWifi);
}

void cambiarCanal(uint8_t c) {
  canalActual = c;
  esp_wifi_set_channel(c, WIFI_SECOND_CHAN_NONE);
}

void iniciarSniffer() {
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  wifi_promiscuous_filter_t filtro = {};
  filtro.filter_mask = WIFI_PROMIS_FILTER_MASK_MGMT | WIFI_PROMIS_FILTER_MASK_DATA;
  esp_wifi_set_promiscuous_filter(&filtro);
  esp_wifi_set_promiscuous_rx_cb(&alRecibir);
  esp_wifi_set_promiscuous(true);
  cambiarCanal(1);
}

void reportarVentana() {
  uint32_t tramas, mgmt, datos, bytes, debiles, dji;
  int32_t rssiSuma;
  int rssiMax, rssiMaxDJI, aparatos = 0, fuertes = 0;

  portENTER_CRITICAL(&candadoWifi);
  tramas = vTramas; mgmt = vMgmt; datos = vDatos; bytes = vBytes;
  debiles = vDebiles; dji = vDJI; rssiSuma = vRssiSuma;
  rssiMax = vRssiMax; rssiMaxDJI = vRssiMaxDJI;
  for (int k = 0; k < MAX_DISP; k++) {
    if (!tabla[k].usado) continue;
    aparatos++;
    if (tabla[k].rssiMax >= RSSI_FUERTE) fuertes++;
    tabla[k].usado = false;  // la tabla se vacia en cada ventana
  }
  vTramas = vMgmt = vDatos = vBytes = vDebiles = vDJI = 0;
  vRssiSuma = 0; vRssiMax = -127; vRssiMaxDJI = -127;
  portEXIT_CRITICAL(&candadoWifi);

  numVentana++;
  int tamProm  = tramas ? (int)(bytes / tramas) : 0;
  int rssiProm = tramas ? (int)(rssiSuma / (int32_t)tramas) : -127;

  char linea[160];
  snprintf(linea, sizeof(linea), "W,%lu,%lu,%d,%lu,%d,%lu,%lu,%d,%d,%d,%lu,%d,%lu,%d",
           (unsigned long)millis(), (unsigned long)numVentana, modoSalto ? 0 : canalActual,
           (unsigned long)tramas, aparatos, (unsigned long)mgmt, (unsigned long)datos,
           tamProm, rssiProm, rssiMax, (unsigned long)debiles, fuertes,
           (unsigned long)dji, rssiMaxDJI);
  imprimirLinea(linea);
}

void leerComandos() {
  if (!Serial.available()) return;
  String s = Serial.readString();
  s.trim();
  if (s.length() == 0) return;
  int c = s.toInt();
  if (c == 0) { modoSalto = true;  imprimirLinea("# Modo WiFi: salto 1-13"); }
  else if (c >= 1 && c <= 13) {
    modoSalto = false; cambiarCanal(c);
    char m[40]; snprintf(m, sizeof(m), "# Modo WiFi: canal fijo %d", c); imprimirLinea(m);
  }
}

// ======================================================================
//  Arranque y ciclo principal
// ======================================================================
void setup() {
  Serial.begin(115200);
  Serial.setTimeout(50);
  candadoSerie = xSemaphoreCreateMutex();
  delay(1500);

  imprimirLinea("# nodo_captura: arrancando");
  if (iniciarNRF()) {
    imprimirLinea("# nRF24 OK");
    xTaskCreatePinnedToCore(tareaEspectro, "espectro", 4096, NULL, 1, NULL, 1);
  } else {
    imprimirLinea("# ERROR: el nRF24 no responde (revisa MISO y la alimentacion). Solo funcionara el WiFi.");
  }
  iniciarSniffer();
  imprimirLinea("# WiFi OK (salto de canales 1-13)");
  imprimirLinea("# Listo");
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
    reportarVentana();
    inicioVentana = ahora;
  }
  delay(2);
}
