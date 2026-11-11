// Prueba de la pantalla TFT 1,8" (ST7735) sola, con los mismos pines de nodo_detector.
// Usa SPI "a mano" (lento y a prueba de cables largos): si esto no muestra nada,
// el problema es de cables o alimentacion, no del firmware del detector.
//
// Conexiones: BL=3V3  CS=15  DC=6  RES=7  SDA=5  SCK=4  VCC=5V  GND=GND
// (VCC a 5V: el modulo trae regulador U1; con 3V3 la pantalla se queda en blanco)
// Libreria: "Adafruit ST7735 and ST7789". Monitor Serie a 115200.
//
// Prueba sola, cada 6 s, las cuatro formas de arrancar el chip (A, B, C y D). En cada una
// muestra rojo, verde, azul y luego la letra en grande. Cuando la pantalla deje de estar
// en blanco, anota la letra que se ve: esa es la que va en nodo_detector.

#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>

const int TFT_SCK = 4, TFT_MOSI = 5, TFT_DC = 6, TFT_RST = 7, TFT_CS = 15;

Adafruit_ST7735 tft(TFT_CS, TFT_DC, TFT_MOSI, TFT_SCK, TFT_RST);   // SPI por software

struct Modo { char letra; const char* nombre; uint8_t tipo; bool esB; };
const Modo MODOS[] = {
  {'A', "INITR_BLACKTAB", INITR_BLACKTAB, false},
  {'B', "INITR_GREENTAB", INITR_GREENTAB, false},
  {'C', "INITR_REDTAB",   INITR_REDTAB,   false},
  {'D', "initB (ST7735B)", 0,             true},
};
const int N_MODOS = sizeof(MODOS) / sizeof(MODOS[0]);

void probar(const Modo& m) {
  Serial.printf("# Modo %c: %s\n", m.letra, m.nombre);
  if (m.esB) tft.initB(); else tft.initR(m.tipo);
  tft.setRotation(1);
  tft.fillScreen(ST77XX_RED);   delay(700);
  tft.fillScreen(ST77XX_GREEN); delay(700);
  tft.fillScreen(ST77XX_BLUE);  delay(700);
  tft.fillScreen(ST77XX_BLACK);
  tft.fillRect(0, 0, 53, 40, ST77XX_RED);
  tft.fillRect(53, 0, 54, 40, ST77XX_GREEN);
  tft.fillRect(107, 0, 53, 40, ST77XX_BLUE);
  tft.setTextColor(ST77XX_WHITE);
  tft.setTextSize(1);
  tft.setCursor(8, 44);   tft.print("ROJO");
  tft.setCursor(64, 44);  tft.print("VERDE");
  tft.setCursor(120, 44); tft.print("AZUL");
  tft.setTextSize(6);
  tft.setTextColor(ST77XX_YELLOW);
  tft.setCursor(62, 62);
  tft.print(m.letra);
  delay(3500);
}

void setup() {
  Serial.begin(115200);
  delay(1500);
  Serial.println("# prueba_pantalla: probando las 4 formas de arrancar la pantalla");
}

void loop() {
  for (int i = 0; i < N_MODOS; i++) probar(MODOS[i]);
}
