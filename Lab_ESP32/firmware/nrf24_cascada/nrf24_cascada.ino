// Cascada del espectro 2,4 GHz con nRF24L01+ y ESP32-S3 (Arduino IDE)
// SOLO RECIBE: el nRF24 queda en modo receptor (PRIM_RX). Nunca transmite.
//
// Como funciona, en simple:
//  - El nRF24 sintoniza cada uno de los 126 canales (2400 a 2525 MHz, 1 MHz cada uno).
//  - En cada canal pregunta "hay algo fuerte aqui?" (registro RPD: si o no).
//  - Repite el barrido 8 veces y cuenta cuantas veces dijo "si" en cada canal (0 a 8).
//  - Imprime una linea: cada caracter es un canal. Mas "lleno" el caracter = mas actividad.
//    Escala:  ' '=0  '.'=1  ':'=2  '-'=3  '='=4  '+'=5  '*'=6  '#'=7  '@'=8
//  - Las lineas de datos van entre barras: |....|  (asi el PC las reconoce despues)
//  - Las lineas que empiezan con # son reglas y notas, no datos.
//
// Conexiones: CE=9  CSN=10  MOSI=11  SCK=12  MISO=13  VCC=3V3  GND=GND

#include <SPI.h>

const int PIN_CE   = 9;
const int PIN_CSN  = 10;
const int PIN_MOSI = 11;
const int PIN_SCK  = 12;
const int PIN_MISO = 13;

const int CANALES           = 126;
const int BARRIDOS_POR_FILA = 8;    // cuantas veces se repite el barrido por cada linea
const int ESPERA_US         = 200;  // tiempo escuchando cada canal (microsegundos)
const char ESCALA[]         = " .:-=+*#@";  // 0..8
const int FILAS_ENTRE_REGLAS = 30;

SPISettings ajustes(1000000, MSBFIRST, SPI_MODE0);

uint8_t leerRegistro(uint8_t reg) {
  SPI.beginTransaction(ajustes);
  digitalWrite(PIN_CSN, LOW);
  SPI.transfer(0x00 | reg);
  uint8_t v = SPI.transfer(0xFF);
  digitalWrite(PIN_CSN, HIGH);
  SPI.endTransaction();
  return v;
}

void escribirRegistro(uint8_t reg, uint8_t valor) {
  SPI.beginTransaction(ajustes);
  digitalWrite(PIN_CSN, LOW);
  SPI.transfer(0x20 | reg);
  SPI.transfer(valor);
  digitalWrite(PIN_CSN, HIGH);
  SPI.endTransaction();
}

void imprimirReglas() {
  char linea[CANALES + 3];
  // Regla 1: decenas de canal (0 = 2400 MHz, 1 = 2410 MHz, ...)
  linea[0] = '#';
  for (int c = 0; c < CANALES; c++) linea[1 + c] = (c % 10 == 0) ? ('0' + (c / 10) % 10) : ' ';
  linea[CANALES + 1] = '#'; linea[CANALES + 2] = 0;
  Serial.println(linea);
  // Regla 2: centros de los canales WiFi 1, 6 y 11 (2412, 2437 y 2462 MHz)
  for (int c = 0; c < CANALES; c++) linea[1 + c] = ' ';
  linea[1 + 12] = '^'; linea[1 + 37] = '^'; linea[1 + 62] = '^';
  Serial.println(linea);
}

bool chipResponde() {
  uint8_t original = leerRegistro(0x05);
  escribirRegistro(0x05, 0x4C);
  bool ok = (leerRegistro(0x05) == 0x4C);
  escribirRegistro(0x05, original);
  return ok;
}

bool listo = false;
int filas = 0;

void setup() {
  Serial.begin(115200);
  delay(2000);
  pinMode(PIN_CE, OUTPUT);
  digitalWrite(PIN_CE, LOW);
  pinMode(PIN_CSN, OUTPUT);
  digitalWrite(PIN_CSN, HIGH);
  SPI.begin(PIN_SCK, PIN_MISO, PIN_MOSI);
  delay(100);

  if (!chipResponde()) {
    Serial.println("# ERROR: el nRF24 no responde. Revisa el cable de MISO y la alimentacion.");
    return;
  }

  escribirRegistro(0x01, 0x00);  // EN_AA: sin acuses automaticos
  escribirRegistro(0x06, 0x07);  // RF_SETUP: 1 Mbps (canal de 1 MHz), ganancia de recepcion
  escribirRegistro(0x00, 0x03);  // CONFIG: encendido + modo RECEPTOR
  delay(5);                      // tiempo de arranque del oscilador

  Serial.println("# Cascada nRF24: cada linea ~1/4 de segundo, cada columna 1 MHz (2400 a 2525 MHz)");
  Serial.println("# Escala: ' '=nada  . : - = + * #  '@'=ocupado siempre.  ^ = WiFi canal 1, 6 y 11");
  listo = true;
}

void loop() {
  if (!listo) { delay(2000); return; }

  uint8_t cuenta[CANALES] = {0};
  for (int b = 0; b < BARRIDOS_POR_FILA; b++) {
    for (int c = 0; c < CANALES; c++) {
      escribirRegistro(0x05, c);       // sintonizar el canal
      digitalWrite(PIN_CE, HIGH);      // escuchar
      delayMicroseconds(ESPERA_US);
      digitalWrite(PIN_CE, LOW);       // dejar de escuchar (guarda el resultado)
      if (leerRegistro(0x09) & 0x01) cuenta[c]++;  // RPD: hubo senal fuerte?
    }
  }

  if (filas % FILAS_ENTRE_REGLAS == 0) imprimirReglas();
  filas++;

  char linea[CANALES + 3];
  linea[0] = '|';
  for (int c = 0; c < CANALES; c++) linea[1 + c] = ESCALA[cuenta[c]];
  linea[CANALES + 1] = '|';
  linea[CANALES + 2] = 0;
  Serial.println(linea);
}
