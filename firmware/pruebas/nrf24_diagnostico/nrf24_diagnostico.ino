// Diagnostico del cableado nRF24L01+ con ESP32-S3 (Arduino IDE)
// No transmite nada por radio. Solo revisa si el nRF24 contesta y por que no.
//
// Conexiones: CE=9  CSN=10  MOSI=11  SCK=12  MISO=13  VCC=3V3  GND=GND

#include <SPI.h>
#include "driver/gpio.h"

const int PIN_CE   = 9;
const int PIN_CSN  = 10;
const int PIN_MOSI = 11;
const int PIN_SCK  = 12;
const int PIN_MISO = 13;

SPISettings ajustes(1000000, MSBFIRST, SPI_MODE0);

uint8_t transferir(bool seleccionar, uint8_t dato) {
  SPI.beginTransaction(ajustes);
  if (seleccionar) digitalWrite(PIN_CSN, LOW);
  uint8_t r = SPI.transfer(dato);
  digitalWrite(PIN_CSN, HIGH);
  SPI.endTransaction();
  return r;
}

void setup() {
  Serial.begin(115200);
  delay(2000);
  pinMode(PIN_CE, OUTPUT);
  digitalWrite(PIN_CE, LOW);
  pinMode(PIN_CSN, OUTPUT);
  digitalWrite(PIN_CSN, HIGH);
  SPI.begin(PIN_SCK, PIN_MISO, PIN_MOSI);
  // Resistencia interna hacia arriba en MISO: si nadie contesta, se lee 0xFF en vez de 0x00
  gpio_pullup_en((gpio_num_t)PIN_MISO);
  delay(100);
}

void loop() {
  Serial.println();
  Serial.println("=== Diagnostico nRF24 ===");

  // Prueba 1: con el nRF24 SIN seleccionar mandamos 0xA5.
  // Solo vuelve 0xA5 si hay un puente directo entre el pin 11 y el 13.
  uint8_t eco = transferir(false, 0xA5);

  // Prueba 2: con el nRF24 seleccionado mandamos NOP (0xFF).
  // Un nRF24 vivo responde su STATUS, normalmente 0x0E.
  uint8_t status = transferir(true, 0xFF);

  Serial.printf("  Eco sin seleccionar = 0x%02X\n", eco);
  Serial.printf("  STATUS              = 0x%02X\n", status);

  if (eco == 0xA5) {
    Serial.println(">>> MODO PUENTE: el pin 11 llega al 13. El lado de la ESP32 funciona bien.");
    Serial.println("    El problema esta en los cables hacia el nRF24 o en el modulo.");
  } else if (status != 0x00 && status != 0xFF) {
    Serial.println(">>> EL nRF24 ESTA VIVO: contesto. El cableado basico esta bien.");
  } else if (status == 0xFF) {
    Serial.println(">>> NADIE CONTESTA (0xFF): el nRF24 no maneja el cable MISO.");
    Serial.println("    Causas probables: no le llega energia (VCC/GND), CSN no llega al pin 10,");
    Serial.println("    MISO suelto, o los nombres de los pines del modulo se leyeron al reves.");
  } else {
    Serial.println(">>> MISO PEGADO A CERO (0x00): algo jala MISO a tierra.");
    Serial.println("    Revisa que el cable que llega al pin 13 venga de MISO y no de GND.");
  }
  delay(3000);
}
