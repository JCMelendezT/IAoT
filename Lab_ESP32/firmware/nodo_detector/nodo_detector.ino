// Nodo detector: el modelo corre DENTRO del ESP32-S3 (IA en el borde). Arduino IDE.
//   1) nRF24L01+  -> filas del espectro 2,4 GHz (126 canales), igual que nodo_captura
//   2) WiFi del ESP32 en modo promiscuo -> resumen del trafico cada 2 s, igual que nodo_captura
//   3) Cada 2 s calcula los rasgos (rasgos_detector.h), usa el modelo (modelo_detector.h)
//      y dice: fondo, bluetooth o dron. Con 2 ventanas seguidas de "dron" se activa la ALERTA.
// SOLO ESCUCHA: el nRF24 queda en modo receptor y el WiFi no se conecta ni transmite.
//
// Placa: "ESP32S3 Dev Module", USB CDC On Boot: Enabled, cable en el conector USB (no el COM).
// Se mira con el Monitor Serie a 115200. Una linea por ventana:
//   [  12] fondo       fondo  97 % | bluetooth   2 % | dron   1 %
//   >>> ALERTA: DRON DETECTADO <<<          (cuando empieza)
//   --- alerta terminada ---                 (cuando termina)
// LED RGB de la placa: verde = fondo, azul = bluetooth, amarillo = dron (1 ventana), rojo = ALERTA.
//
// Conexiones del nRF24: CE=9  CSN=10  MOSI=11  SCK=12  MISO=13  VCC=3V3  GND=GND (Lab_ESP32/PINOUT.md)
// Si se reentrena, entrenar.py copia aqui el modelo_detector.h nuevo: solo hay que volver a subir.

#include <SPI.h>
#include <WiFi.h>
#include "esp_wifi.h"
#include "rasgos_detector.h"
#include "modelo_detector.h"

// ---------- Pines del nRF24 ----------
const int PIN_CE   = 9;
const int PIN_CSN  = 10;
const int PIN_MOSI = 11;
const int PIN_SCK  = 12;
const int PIN_MISO = 13;

// ---------- Ajustes (los mismos de nodo_captura: el modelo se entreno asi) ----------
const int BARRIDOS_POR_FILA = 8;
const int ESPERA_US         = 200;
const int VENTANA_MS        = 2000;
const int SALTO_MS          = 150;   // 13 canales WiFi x 150 ms ~ 2 s
const int MAX_DISP          = 64;
const int RSSI_DEBIL        = -75;
const int RSSI_FUERTE       = -55;

// ---------- Alerta ----------
const int VENTANAS_PARA_ALERTA = 2;  // "dron" seguidas para encender la alerta
const int VENTANAS_PARA_APAGAR = 2;  // ventanas sin dron seguidas para apagarla
const uint8_t BRILLO_LED       = 30; // 0..255
// LED RGB de la placa: GPIO 48 en la DevKitC-1 v1.0 (lo que trae RGB_BUILTIN).
// Si no prende, algunas placas (v1.1) lo tienen en el 38: cambia la linea por  = 38;
#ifdef RGB_BUILTIN
const uint8_t PIN_LED_RGB      = RGB_BUILTIN;
#endif

SPISettings ajustesSPI(1000000, MSBFIRST, SPI_MODE0);

// ======================================================================
//  LED RGB integrado (si la placa lo tiene)
// ======================================================================
void led(uint8_t r, uint8_t g, uint8_t b) {
#ifdef RGB_BUILTIN
  r = (uint16_t)r * BRILLO_LED / 255; g = (uint16_t)g * BRILLO_LED / 255; b = (uint16_t)b * BRILLO_LED / 255;
#if ESP_ARDUINO_VERSION_MAJOR >= 3
  rgbLedWrite(PIN_LED_RGB, r, g, b);
#else
  neopixelWrite(PIN_LED_RGB, r, g, b);
#endif
#endif
}

// ======================================================================
//  Oido 1: nRF24 (tarea propia). Cada fila se suma a la ventana en curso.
// ======================================================================
VentanaEspectro espectroEnCurso;
portMUX_TYPE candadoEsp = portMUX_INITIALIZER_UNLOCKED;

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
  uint8_t fila[RD_CANALES];
  for (;;) {
    memset(fila, 0, sizeof(fila));
    for (int b = 0; b < BARRIDOS_POR_FILA; b++) {
      for (int c = 0; c < RD_CANALES; c++) {
        escribirRegistro(0x05, c);
        digitalWrite(PIN_CE, HIGH);
        delayMicroseconds(ESPERA_US);
        digitalWrite(PIN_CE, LOW);
        if (leerRegistro(0x09) & 0x01) fila[c]++;
      }
      vTaskDelay(1);
    }
    portENTER_CRITICAL(&candadoEsp);
    espectroAgregarFila(espectroEnCurso, fila);
    portEXIT_CRITICAL(&candadoEsp);
  }
}

// ======================================================================
//  Oido 2: sniffer WiFi (identico a nodo_captura, sin imprimir)
// ======================================================================
struct Disp {
  bool     usado;
  uint8_t  mac[6];
  int8_t   rssiMax;
};

Disp tabla[MAX_DISP];
portMUX_TYPE candadoWifi = portMUX_INITIALIZER_UNLOCKED;
uint32_t vTramas = 0, vMgmt = 0, vDatos = 0, vBytes = 0, vDebiles = 0;
int32_t  vRssiSuma = 0;
int      vRssiMax = -127;
uint8_t  canalActual = 1;

void alRecibir(void* buf, wifi_promiscuous_pkt_type_t tipo) {
  if (tipo != WIFI_PKT_MGMT && tipo != WIFI_PKT_DATA) return;
  const wifi_promiscuous_pkt_t* p = (const wifi_promiscuous_pkt_t*)buf;
  int largo = p->rx_ctrl.sig_len;
  if (largo < 24) return;
  const uint8_t* emisor = p->payload + 10;
  int rssi = p->rx_ctrl.rssi;

  portENTER_CRITICAL(&candadoWifi);
  vTramas++;
  vBytes += largo;
  vRssiSuma += rssi;
  if (rssi > vRssiMax) vRssiMax = rssi;
  if (rssi < RSSI_DEBIL) vDebiles++;
  if (tipo == WIFI_PKT_MGMT) vMgmt++; else vDatos++;
  int libre = -1, i = -1;
  for (int k = 0; k < MAX_DISP; k++) {
    if (tabla[k].usado) { if (memcmp(tabla[k].mac, emisor, 6) == 0) { i = k; break; } }
    else if (libre < 0) libre = k;
  }
  if (i < 0 && libre >= 0) {
    i = libre;
    tabla[i].usado = true;
    memcpy(tabla[i].mac, emisor, 6);
    tabla[i].rssiMax = -127;
  }
  if (i >= 0 && rssi > tabla[i].rssiMax) tabla[i].rssiMax = rssi;
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

// Cierra la ventana WiFi: devuelve el resumen y deja los contadores en cero
VentanaWifi cerrarVentanaWifi() {
  uint32_t tramas, mgmt, datos, bytes, debiles;
  int32_t rssiSuma;
  int rssiMax, aparatos = 0, fuertes = 0;
  portENTER_CRITICAL(&candadoWifi);
  tramas = vTramas; mgmt = vMgmt; datos = vDatos; bytes = vBytes;
  debiles = vDebiles; rssiSuma = vRssiSuma; rssiMax = vRssiMax;
  for (int k = 0; k < MAX_DISP; k++) {
    if (!tabla[k].usado) continue;
    aparatos++;
    if (tabla[k].rssiMax >= RSSI_FUERTE) fuertes++;
    tabla[k].usado = false;
  }
  vTramas = vMgmt = vDatos = vBytes = vDebiles = 0;
  vRssiSuma = 0; vRssiMax = -127;
  portEXIT_CRITICAL(&candadoWifi);

  VentanaWifi w;
  w.tramas = tramas; w.aparatos = aparatos; w.mgmt = mgmt; w.datos = datos;
  w.tamProm  = tramas ? (int)(bytes / tramas) : 0;
  w.rssiProm = tramas ? (int)(rssiSuma / (int32_t)tramas) : -127;
  w.rssiMax  = rssiMax; w.debiles = debiles; w.fuertes = fuertes;
  return w;
}

// ======================================================================
//  Clasificacion y alerta
// ======================================================================
int mapaRasgos[DETECTOR_N_RASGOS];
bool modeloListo = false;
uint32_t numVentana = 0;
int seguidasDron = 0, seguidasSinDron = 0;
bool alertaActiva = false;

void clasificarVentana() {
  VentanaWifi w = cerrarVentanaWifi();
  VentanaEspectro e;
  portENTER_CRITICAL(&candadoEsp);
  e = espectroEnCurso;
  espectroLimpiar(espectroEnCurso);
  portEXIT_CRITICAL(&candadoEsp);
  numVentana++;

  float todos[RD_N_TODOS], x[DETECTOR_N_RASGOS], prob[DETECTOR_N_CLASES];
  if (!modeloListo || !rasgosCalcular(w, e, todos)) {
    Serial.printf("[%4lu] (sin espectro suficiente: %u filas)\n", (unsigned long)numVentana, e.filas);
    return;
  }
  rasgosElegir(todos, mapaRasgos, DETECTOR_N_RASGOS, x);
  unsigned long t0 = micros();
  int clase = detectorClasificar(x, prob);
  unsigned long usModelo = micros() - t0;
  bool esDron = (clase == 2);

  if (esDron) { seguidasDron++; seguidasSinDron = 0; }
  else        { seguidasSinDron++; seguidasDron = 0; }

  Serial.printf("[%4lu] %-10s  fondo %3d %% | bluetooth %3d %% | dron %3d %%   (%lu us)\n",
                (unsigned long)numVentana, DETECTOR_CLASES[clase],
                (int)lroundf(prob[0] * 100), (int)lroundf(prob[1] * 100), (int)lroundf(prob[2] * 100),
                usModelo);

  if (!alertaActiva && seguidasDron >= VENTANAS_PARA_ALERTA) {
    alertaActiva = true;
    Serial.println(">>> ALERTA: DRON DETECTADO <<<");
  } else if (alertaActiva && seguidasSinDron >= VENTANAS_PARA_APAGAR) {
    alertaActiva = false;
    Serial.println("--- alerta terminada ---");
  }

  if (alertaActiva)   led(255, 0, 0);     // rojo
  else if (esDron)    led(255, 160, 0);   // amarillo: una sola ventana, aun no es alerta
  else if (clase == 1) led(0, 0, 255);    // azul
  else                led(0, 255, 0);     // verde
}

// ======================================================================
//  Arranque y ciclo principal
// ======================================================================
unsigned long inicioVentana = 0, ultimoSalto = 0;

void setup() {
  Serial.begin(115200);
#if ARDUINO_USB_CDC_ON_BOOT && ARDUINO_USB_MODE
  Serial.setTxTimeoutMs(0);   // sin PC conectado, no se queda esperando al imprimir
#endif
  delay(1500);
  led(255, 255, 255);
  Serial.println("# nodo_detector: arrancando");

  const char* falta = rasgosPrepararMapa(DETECTOR_RASGOS, DETECTOR_N_RASGOS, mapaRasgos);
  if (falta) {
    Serial.printf("# ERROR: el modelo pide el rasgo '%s' y este firmware no lo calcula\n", falta);
  } else {
    modeloListo = true;
    Serial.printf("# Modelo: %d rasgos, clases fondo / bluetooth / dron\n", DETECTOR_N_RASGOS);
  }

  if (iniciarNRF()) {
    Serial.println("# nRF24 OK");
    espectroLimpiar(espectroEnCurso);
    xTaskCreatePinnedToCore(tareaEspectro, "espectro", 4096, NULL, 1, NULL, 1);
  } else {
    Serial.println("# ERROR: el nRF24 no responde (revisa MISO y la alimentacion). Sin espectro no hay deteccion.");
    led(255, 0, 255);  // morado: falla del nRF24
  }
  iniciarSniffer();
  Serial.println("# WiFi OK (salto de canales 1-13)");
  Serial.println("# Listo: una linea cada 2 s");
  inicioVentana = ultimoSalto = millis();
}

void loop() {
  unsigned long ahora = millis();
  if (ahora - ultimoSalto >= SALTO_MS) {
    cambiarCanal(canalActual % 13 + 1);
    ultimoSalto = ahora;
  }
  if (ahora - inicioVentana >= VENTANA_MS) {
    inicioVentana = ahora;
    clasificarVentana();
  }
  delay(2);
}
