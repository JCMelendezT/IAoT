// Prueba de vida del nRF24L01+ con ESP32-S3 (Arduino IDE)
// Solo lee y escribe registros internos. NO transmite nada por radio.
//
// Conexiones (ver Lab_ESP32/PINOUT.md):
//   CE=9  CSN=10  MOSI=11  SCK=12  MISO=13  IRQ sin conectar
//   VCC=3V3 (NUNCA 5V), GND=GND, condensador 100 uF entre VCC y GND

#include <SPI.h>

const int PIN_CE   = 9;
const int PIN_CSN  = 10;
const int PIN_MOSI = 11;
const int PIN_SCK  = 12;
const int PIN_MISO = 13;

// 1 MHz: lento y seguro, porque los cables Dupont son largos
SPISettings ajustes(1000000, MSBFIRST, SPI_MODE0);

// Pregunta al nRF24 el valor de un registro
uint8_t leerRegistro(uint8_t reg) {
  SPI.beginTransaction(ajustes);
  digitalWrite(PIN_CSN, LOW);        // "oye, te estoy hablando"
  SPI.transfer(0x00 | reg);          // comando: leer registro
  uint8_t valor = SPI.transfer(0xFF);
  digitalWrite(PIN_CSN, HIGH);       // "ya termine"
  SPI.endTransaction();
  return valor;
}

// Le guarda un valor en un registro
void escribirRegistro(uint8_t reg, uint8_t valor) {
  SPI.beginTransaction(ajustes);
  digitalWrite(PIN_CSN, LOW);
  SPI.transfer(0x20 | reg);          // comando: escribir registro
  SPI.transfer(valor);
  digitalWrite(PIN_CSN, HIGH);
  SPI.endTransaction();
}

void imprimir(const char* nombre, uint8_t reg, const char* esperado) {
  uint8_t v = leerRegistro(reg);
  Serial.printf("  %-9s = 0x%02X   (normal: %s)\n", nombre, v, esperado);
}

void setup() {
  Serial.begin(115200);
  delay(2000);                       // tiempo para abrir el monitor serie

  pinMode(PIN_CE, OUTPUT);
  digitalWrite(PIN_CE, LOW);         // radio en reposo: ni recibe ni transmite
  pinMode(PIN_CSN, OUTPUT);
  digitalWrite(PIN_CSN, HIGH);
  SPI.begin(PIN_SCK, PIN_MISO, PIN_MOSI);
  delay(100);                        // el nRF24 necesita un momento al encender
}

void loop() {
  Serial.println();
  Serial.println("=== Prueba de vida nRF24L01+ ===");
  imprimir("CONFIG",   0x00, "0x08");
  imprimir("EN_AA",    0x01, "0x3F");
  imprimir("SETUP_AW", 0x03, "0x03");
  imprimir("RF_CH",    0x05, "0x02");
  imprimir("RF_SETUP", 0x06, "0x0E o 0x0F");
  imprimir("STATUS",   0x07, "0x0E");

  // La prueba clave: escribir un valor y leerlo de vuelta
  uint8_t original = leerRegistro(0x05);
  escribirRegistro(0x05, 0x4C);
  uint8_t leido = leerRegistro(0x05);
  escribirRegistro(0x05, original);  // lo dejamos como estaba

  Serial.printf("Prueba de escritura: escribi 0x4C, lei 0x%02X\n", leido);
  if (leido == 0x4C) {
    Serial.println(">>> PASO: la placa y el nRF24 se hablan bien.");
  } else if (leido == 0x00 || leido == 0xFF) {
    Serial.println(">>> FALLO: no hay respuesta. Revisa VCC/GND, CSN y que MOSI/MISO no esten cruzados.");
  } else {
    Serial.println(">>> FALLO: respuesta rara. Revisa SCK y que los cables esten firmes.");
  }
  delay(3000);                       // repite cada 3 segundos
}
