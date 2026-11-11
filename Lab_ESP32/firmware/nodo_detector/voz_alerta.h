// Aviso sonoro de alerta con el modulo ISD1820 y su parlante.
// El ISD1820 no genera sonidos: reproduce lo que se le grabo con el boton REC (hasta 10 s).
// Graba un pitido, una sirena o una voz; un pulso en P-E lo reproduce completo.
//
// ISD1820 (conector J1): VCC=3V3  GND=GND  P-E=16. FT, P-L y REC sin conectar.
// Quitar los puentes FT y P-E del modulo (con FT el parlante repite el microfono y con
// P-E el sonido se repite sin parar). El parlante va al conector SP del modulo.
#pragma once

const int VOZ_PLAY = 16;                     // P-E del ISD1820
const unsigned long VOZ_PULSO_MS   = 80;
const unsigned long VOZ_REPETIR_MS = 5000;   // mientras dure la alerta, repite el sonido

unsigned long vozInicio = 0, vozUltima = 0;
bool vozPulsoActivo = false;

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
