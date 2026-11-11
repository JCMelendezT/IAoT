// Prueba de la pantalla TFT 1,8" (ST7735) sola, con los mismos pines de nodo_detector.
// Usa SPI "a mano" (lento y a prueba de cables largos): si esto no muestra nada,
// el problema es de cables o alimentacion, no del firmware del detector.
//
// Conexiones: BL=3V3  CS=15  DC=6  RES=7  SDA=5  SCK=4  VCC=3V3  GND=GND
// Libreria: "Adafruit ST7735 and ST7789". Monitor Serie a 115200.
//
// Debe verse: pantalla ROJA, luego VERDE, luego AZUL (1 s cada una) y despues tres
// franjas con sus nombres y el texto "PANTALLA OK". Si los colores no coinciden con
// los nombres, cambia INITR_BLACKTAB por INITR_GREENTAB (y anota cual sirvio).

#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>

const int TFT_SCK = 4, TFT_MOSI = 5, TFT_DC = 6, TFT_RST = 7, TFT_CS = 15;
#define TFT_TIPO INITR_BLACKTAB

Adafruit_ST7735 tft(TFT_CS, TFT_DC, TFT_MOSI, TFT_SCK, TFT_RST);   // SPI por software

void setup() {
  Serial.begin(115200);
  delay(1500);
  Serial.println("# prueba_pantalla: iniciando la pantalla...");
  tft.initR(TFT_TIPO);
  tft.setRotation(1);
  Serial.println("# Rojo");  tft.fillScreen(ST77XX_RED);   delay(1000);
  Serial.println("# Verde"); tft.fillScreen(ST77XX_GREEN); delay(1000);
  Serial.println("# Azul");  tft.fillScreen(ST77XX_BLUE);  delay(1000);

  tft.fillScreen(ST77XX_BLACK);
  tft.fillRect(0, 0, 53, 70, ST77XX_RED);
  tft.fillRect(53, 0, 54, 70, ST77XX_GREEN);
  tft.fillRect(107, 0, 53, 70, ST77XX_BLUE);
  tft.setTextColor(ST77XX_WHITE);
  tft.setTextSize(1);
  tft.setCursor(8, 74);   tft.print("ROJO");
  tft.setCursor(64, 74);  tft.print("VERDE");
  tft.setCursor(120, 74); tft.print("AZUL");
  tft.setTextSize(2);
  tft.setTextColor(ST77XX_GREEN);
  tft.setCursor(10, 96);
  tft.print("PANTALLA OK");
  Serial.println("# Listo: si ves las franjas con su nombre, la pantalla y los cables estan bien.");
}

void loop() {
  // Un punto que parpadea en la esquina para saber que la placa sigue viva
  static bool encendido = false;
  encendido = !encendido;
  tft.fillCircle(152, 120, 4, encendido ? ST77XX_WHITE : ST77XX_BLACK);
  delay(500);
}
