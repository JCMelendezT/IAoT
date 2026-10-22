/*
  devkit_latido — Prueba de red de la Practica 1 (placa: ESP32 devkit, "ESP32 Dev Module")

  Que hace:
    1. Se conecta al WiFi (nombre y clave en secrets.h).
    2. Cada 5 segundos le manda al servidor un "estoy vivo" (latido)
       a la direccion  http://SERVIDOR_IP:SERVIDOR_PUERTO/api/latido
    3. En el Monitor Serie (115200) dice si cada latido llego o fallo.

  Antes de subirlo: abre la pestana secrets.h y pon en SERVIDOR_IP
  la IP que te muestra el servidor de prueba en Ubuntu.
*/
#include <WiFi.h>
#include <HTTPClient.h>
#include "secrets.h"

const char* NODO = "pasarela-1";
const unsigned long CADA_MS = 5000;

unsigned long contador = 0;
unsigned long ultimoEnvio = 0;

void conectarWiFi() {
  Serial.printf("Conectando a \"%s\"", WIFI_NOMBRE);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_NOMBRE, WIFI_CLAVE);
  unsigned long inicio = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - inicio < 20000) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf(">>> WiFi OK. IP del ESP32: %s  Canal: %d  Senal: %d dBm\n",
                  WiFi.localIP().toString().c_str(), WiFi.channel(), (int)WiFi.RSSI());
    Serial.printf(">>> Enviare latidos a http://%s:%d/api/latido\n", SERVIDOR_IP, SERVIDOR_PUERTO);
  } else {
    Serial.println(">>> No se pudo conectar al WiFi. Reintento en 5 s.");
  }
}

void enviarLatido() {
  contador++;
  char url[96];
  snprintf(url, sizeof(url), "http://%s:%d/api/latido", SERVIDOR_IP, SERVIDOR_PUERTO);

  char json[160];
  snprintf(json, sizeof(json),
           "{\"nodo\":\"%s\",\"contador\":%lu,\"ms\":%lu,\"rssi\":%d}",
           NODO, contador, millis(), (int)WiFi.RSSI());

  HTTPClient http;
  http.setTimeout(3000);
  http.begin(url);
  http.addHeader("Content-Type", "application/json");
  int codigo = http.POST(json);

  if (codigo == 200) {
    Serial.printf("Latido %lu -> LLEGO (el servidor respondio OK)\n", contador);
  } else if (codigo > 0) {
    Serial.printf("Latido %lu -> el servidor respondio con codigo %d\n", contador, codigo);
  } else {
    Serial.printf("Latido %lu -> FALLO: %s\n", contador, http.errorToString(codigo).c_str());
  }
  http.end();
}

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println();
  Serial.println("=== devkit_latido: prueba ESP32 -> servidor ===");
  conectarWiFi();
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    conectarWiFi();
    delay(5000);
    return;
  }
  if (millis() - ultimoEnvio >= CADA_MS) {
    ultimoEnvio = millis();
    enviarLatido();
  }
}
