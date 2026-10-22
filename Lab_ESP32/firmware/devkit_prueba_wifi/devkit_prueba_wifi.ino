// Prueba de conexion WiFi del ESP32 devkit (Arduino IDE)
// Se conecta a tu red y muestra: IP, banda/canal, fuerza de la senal y el router al que se conecto.
//
// ANTES DE SUBIRLO: abre la pestana "secrets.h" y escribe el nombre y la clave de tu WiFi.
// secrets.h NUNCA se sube a GitHub (esta en .gitignore).

#include <WiFi.h>
#include "secrets.h"

void setup() {
  Serial.begin(115200);
  delay(1500);
  Serial.println();
  Serial.printf("Conectando a \"%s\"...\n", WIFI_NOMBRE);

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_NOMBRE, WIFI_CLAVE);

  unsigned long inicio = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - inicio < 20000) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println(">>> CONECTADO");
    Serial.print("  IP del ESP32 : "); Serial.println(WiFi.localIP());
    Serial.print("  Puerta (router): "); Serial.println(WiFi.gatewayIP());
    Serial.printf("  Canal        : %d  (canales 1 a 13 = 2,4 GHz)\n", WiFi.channel());
    Serial.printf("  Senal (RSSI) : %d dBm\n", WiFi.RSSI());
    Serial.print("  Router (BSSID): "); Serial.println(WiFi.BSSIDstr());
  } else {
    Serial.println(">>> NO SE PUDO CONECTAR en 20 s.");
    Serial.println("  Revisa el nombre y la clave en secrets.h (mayusculas y minusculas cuentan).");
    Serial.println("  Redes de 2,4 GHz que ve el ESP32:");
    int n = WiFi.scanNetworks();
    for (int i = 0; i < n; i++)
      Serial.printf("    %-32s canal %2d  senal %d dBm\n", WiFi.SSID(i).c_str(), WiFi.channel(i), WiFi.RSSI(i));
  }
}

void loop() {
  delay(5000);
  if (WiFi.status() == WL_CONNECTED)
    Serial.printf("Sigo conectado. Senal: %d dBm\n", WiFi.RSSI());
  else
    Serial.println("Conexion perdida.");
}
