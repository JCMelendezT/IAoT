// Pantalla tactica (TFT 1,8" ST7735, 160x128 acostada) y voz de alerta (ISD1820).
// La deteccion no depende de esto: si la pantalla o el modulo de voz no estan, el
// detector funciona igual por el Monitor Serie y el LED.
//
// Pantalla (bus SPI propio, separado del nRF24):
//   VCC=3V3  GND=GND  CS=15  RESET=7  A0/DC=6  SDA=5  SCK=4  LED=3V3
// ISD1820:  VCC=3V3  GND=GND  P-E=16   (REC, P-L y FT sin conectar; el mensaje se graba con el boton REC)
//
// Librerias (Arduino IDE > Gestor de librerias): "Adafruit ST7735 and ST7789" y "Adafruit GFX".
#pragma once
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <SPI.h>

// ---------- Pines ----------
const int TFT_SCK  = 4;
const int TFT_MOSI = 5;
const int TFT_DC   = 6;
const int TFT_RST  = 7;
const int TFT_CS   = 15;
const int VOZ_PLAY = 16;           // P-E del ISD1820: un pulso reproduce el mensaje completo

// Si los colores salen raros o hay una franja de basura en un borde, prueba
// INITR_GREENTAB o INITR_REDTAB en vez de INITR_BLACKTAB.
#define TFT_TIPO INITR_BLACKTAB

const unsigned long VOZ_PULSO_MS    = 80;
const unsigned long VOZ_REPETIR_MS  = 12000;  // mientras dure la alerta, repite el aviso

// ---------- Colores (RGB565) ----------
#define C_FONDO    0x0000
#define C_VERDE    0x07E0
#define C_VERDE_M  0x0400
#define C_VERDE_O  0x01E0
#define C_AMBAR    0xFD20
#define C_ROJO     0xF800
#define C_ROJO_O   0x5000
#define C_BLANCO   0xFFFF
#define C_GRIS     0x4208

// ---------- Geometria (pantalla acostada, 160 x 128) ----------
// y: 0-10 barra superior | 18-67 cascada | 72-79 escala | 80-87 contadores | 88-127 estado
const int CAS_X = 2, CAS_Y = 18, CAS_ALTO = 50;      // cascada: 126 canales de ancho
const int BAR_X = 138, BAR_Y = 22, BAR_ANCHO = 14, BAR_ALTO = 42;
const int CAJA_Y = 88;

SPIClass spiPantalla(HSPI);
Adafruit_ST7735 tft(&spiPantalla, TFT_CS, TFT_DC, TFT_RST);

uint8_t  cascada[CAS_ALTO][RD_CANALES];
int      cascadaInicio = 0;                // fila mas nueva
uint16_t paleta[9];
bool     pantallaLista = false;

// Estado que se muestra
int      pClase = 0, pProbDron = 0, pAparatos = 0;
uint32_t pVentana = 0, pAlertas = 0;
bool     pAlerta = false, pParpadeo = false;
unsigned long pUltimoParpadeo = 0, pUltimoReloj = 0;

// Voz
unsigned long vozInicio = 0, vozUltima = 0;
bool vozPulsoActivo = false;

// ======================================================================
//  Voz
// ======================================================================
void vozIniciar() {
  pinMode(VOZ_PLAY, OUTPUT);
  digitalWrite(VOZ_PLAY, LOW);
}

void vozReproducir() {
  digitalWrite(VOZ_PLAY, HIGH);
  vozPulsoActivo = true;
  vozInicio = vozUltima = millis();
}

// Llamar en cada vuelta del loop
void vozActualizar(bool alerta) {
  unsigned long ahora = millis();
  if (vozPulsoActivo && ahora - vozInicio >= VOZ_PULSO_MS) {
    digitalWrite(VOZ_PLAY, LOW);
    vozPulsoActivo = false;
  }
  if (alerta && !vozPulsoActivo && ahora - vozUltima >= VOZ_REPETIR_MS) vozReproducir();
}

// ======================================================================
//  Pantalla: piezas
// ======================================================================
void textoEn(int x, int y, const char* s, uint16_t color, uint16_t fondo, uint8_t tam = 1) {
  tft.setTextSize(tam);
  tft.setTextColor(color, fondo);
  tft.setCursor(x, y);
  tft.print(s);
}

int xCanal(int c) { return CAS_X + c; }

void dibujarMarco() {
  tft.fillScreen(C_FONDO);
  // Barra superior
  tft.fillRect(0, 0, 160, 11, C_VERDE_O);
  textoEn(3, 2, "CENTINELA RF", C_VERDE, C_VERDE_O);
  // Corchetes de la banda del dron sobre la cascada
  int a = xCanal(RD_BANDA_DRON_INI), b = xCanal(RD_BANDA_DRON_FIN - 1);
  tft.drawFastHLine(a, 14, b - a + 1, C_AMBAR);
  tft.drawFastVLine(a, 14, 3, C_AMBAR);
  tft.drawFastVLine(b, 14, 3, C_AMBAR);
  // Borde de la cascada y escala de frecuencia
  tft.drawRect(CAS_X - 1, CAS_Y - 1, RD_CANALES + 2, CAS_ALTO + 2, C_VERDE_M);
  const int marcas[] = {0, 25, 50, 75, 100, 125};
  for (int i = 0; i < 6; i++) tft.drawFastVLine(xCanal(marcas[i]), CAS_Y + CAS_ALTO + 1, 3, C_VERDE_M);
  const int yEscala = CAS_Y + CAS_ALTO + 4;
  textoEn(CAS_X, yEscala, "2.40", C_VERDE_M, C_FONDO);
  textoEn(xCanal(50) - 12, yEscala, "2.45", C_VERDE_M, C_FONDO);
  textoEn(xCanal(100) - 12, yEscala, "2.50", C_VERDE_M, C_FONDO);
  textoEn(xCanal(125) - 5, yEscala, "G", C_VERDE_M, C_FONDO);
  // Barra de probabilidad de dron
  textoEn(BAR_X - 5, 13, "DRON", C_VERDE_M, C_FONDO);
  tft.drawRect(BAR_X - 1, BAR_Y - 1, BAR_ANCHO + 2, BAR_ALTO + 2, C_VERDE_M);
}

void dibujarCascada() {
  static uint16_t linea[RD_CANALES];
  tft.startWrite();
  tft.setAddrWindow(CAS_X, CAS_Y, RD_CANALES, CAS_ALTO);
  for (int r = 0; r < CAS_ALTO; r++) {               // arriba = ahora
    const uint8_t* f = cascada[(cascadaInicio + r) % CAS_ALTO];
    for (int c = 0; c < RD_CANALES; c++) linea[c] = paleta[f[c] > 8 ? 8 : f[c]];
    tft.writePixels(linea, RD_CANALES);
  }
  tft.endWrite();
}

void dibujarBarra() {
  int alto = BAR_ALTO * pProbDron / 100;
  uint16_t color = pProbDron >= 80 ? C_ROJO : (pProbDron >= 50 ? C_AMBAR : C_VERDE);
  tft.fillRect(BAR_X, BAR_Y, BAR_ANCHO, BAR_ALTO - alto, C_FONDO);
  tft.fillRect(BAR_X, BAR_Y + BAR_ALTO - alto, BAR_ANCHO, alto, color);
  char s[6];
  snprintf(s, sizeof(s), "%3d%%", pProbDron);
  textoEn(BAR_X - 6, BAR_Y + BAR_ALTO + 4, s, color, C_FONDO);
}

void dibujarCaja() {
  uint16_t fondo, color;
  const char* titulo;
  const char* sub;
  if (pAlerta) {
    fondo = pParpadeo ? C_ROJO : C_ROJO_O; color = C_BLANCO;
    titulo = "AMENAZA: DRON"; sub = "CONTROL 2.4 GHZ EN ZONA";
  } else if (pClase == 2) {
    fondo = C_FONDO; color = C_AMBAR; titulo = "POSIBLE DRON"; sub = "CONFIRMANDO...";
  } else if (pClase == 1) {
    fondo = C_FONDO; color = C_VERDE; titulo = "BLUETOOTH"; sub = "RF CIVIL - SIN AMENAZA";
  } else {
    fondo = C_FONDO; color = C_VERDE; titulo = "DESPEJADO"; sub = "ESPECTRO NORMAL";
  }
  tft.fillRect(1, CAJA_Y + 1, 158, 38, fondo);
  tft.drawRect(0, CAJA_Y, 160, 40, pAlerta ? C_ROJO : C_VERDE_M);
  int ancho = strlen(titulo) * 12;
  textoEn((160 - ancho) / 2, CAJA_Y + 5, titulo, color, fondo, 2);
  textoEn((160 - (int)strlen(sub) * 6) / 2, CAJA_Y + 24, sub, pAlerta ? C_BLANCO : C_VERDE_M, fondo);
  char s[32];
  snprintf(s, sizeof(s), "WIFI %-3d  #%-5lu  ALR %lu", pAparatos, (unsigned long)pVentana, (unsigned long)pAlertas);
  textoEn(4, CAJA_Y - 8, s, C_VERDE_M, C_FONDO);
}

void dibujarReloj() {
  unsigned long s = millis() / 1000;
  char t[16];
  snprintf(t, sizeof(t), "T+%02lu:%02lu:%02lu", s / 3600, (s / 60) % 60, s % 60);
  textoEn(160 - 3 - 6 * (int)strlen(t), 2, t, C_VERDE, C_VERDE_O);
}

// ======================================================================
//  Pantalla: lo que llama el sketch
// ======================================================================
void pantallaIniciar() {
  spiPantalla.begin(TFT_SCK, -1, TFT_MOSI, TFT_CS);
  tft.initR(TFT_TIPO);
  tft.setRotation(1);
  tft.fillScreen(C_FONDO);
  // Paleta tipo fosforo: negro -> verde -> amarillo -> blanco
  const uint8_t r[9] = {0, 0, 0, 0, 40, 120, 220, 255, 255};
  const uint8_t g[9] = {0, 40, 90, 150, 210, 240, 255, 255, 255};
  const uint8_t b[9] = {0, 0, 10, 20, 30, 40, 60, 140, 255};
  for (int i = 0; i < 9; i++) paleta[i] = tft.color565(r[i], g[i], b[i]);
  memset(cascada, 0, sizeof(cascada));
  pantallaLista = true;
}

// Pantalla de arranque: una linea por chequeo
int arranqueY = 34;
void pantallaArranqueTitulo() {
  tft.fillScreen(C_FONDO);
  textoEn(10, 8, "CENTINELA RF", C_VERDE, C_FONDO, 2);
  textoEn(10, 24, "DETECTOR PASIVO 2.4 GHZ", C_VERDE_M, C_FONDO);
  tft.drawFastHLine(10, 32, 140, C_VERDE_M);
  arranqueY = 38;
}

void pantallaArranqueLinea(const char* que, bool ok, const char* detalle = "") {
  char s[40];
  snprintf(s, sizeof(s), "%-8s %s %s", que, ok ? "[ OK ]" : "[FALLA]", detalle);
  textoEn(10, arranqueY, s, ok ? C_VERDE : C_ROJO, C_FONDO);
  arranqueY += 11;
}

void pantallaArranqueFin() {
  textoEn(10, arranqueY + 6, "IA EN EL BORDE: ACTIVA", C_AMBAR, C_FONDO);
  delay(1500);
  dibujarMarco();
  dibujarCascada();
  dibujarBarra();
  dibujarCaja();
  dibujarReloj();
}

// Una fila nueva del nRF24 entra arriba de la cascada
void pantallaAgregarFila(const uint8_t* fila) {
  cascadaInicio = (cascadaInicio + CAS_ALTO - 1) % CAS_ALTO;
  memcpy(cascada[cascadaInicio], fila, RD_CANALES);
  if (pantallaLista) dibujarCascada();
}

void pantallaResultado(int clase, float probDron, bool alerta, int aparatos, uint32_t ventana, uint32_t alertas) {
  pClase = clase; pProbDron = (int)lroundf(probDron * 100); pAlerta = alerta;
  pAparatos = aparatos; pVentana = ventana; pAlertas = alertas;
  if (!pantallaLista) return;
  dibujarBarra();
  dibujarCaja();
}

// Llamar en cada vuelta del loop: parpadeo de la alerta y reloj
void pantallaActualizar() {
  if (!pantallaLista) return;
  unsigned long ahora = millis();
  if (pAlerta && ahora - pUltimoParpadeo >= 400) {
    pUltimoParpadeo = ahora;
    pParpadeo = !pParpadeo;
    dibujarCaja();
  }
  if (ahora - pUltimoReloj >= 1000) {
    pUltimoReloj = ahora;
    dibujarReloj();
  }
}
