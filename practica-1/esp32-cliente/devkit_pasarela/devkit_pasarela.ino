/*
  devkit_pasarela — Practica 1 (placa: ESP32 devkit, "ESP32 Dev Module")

  Hace de "puente" entre el nodo S3 y el servidor:
    1. Se conecta al WiFi (datos en secrets.h, igual que devkit_latido).
    2. Escucha el cable del S3 en GPIO 16 (RX2).
    3. Cada linea que llega la convierte en JSON y la sube al servidor:
         V,...  -> POST /api/ventanas   (nodo "nodo-s3")
         L,...  -> POST /api/latido     (nodo "nodo-s3", sin senal WiFi propia)
         E,...  -> POST /api/eventos    (nodo "nodo-s3")
    4. Ademas manda su propio latido cada 5 s (nodo "pasarela-1").

  Cables: GPIO 17 del S3 -> GPIO 16 (RX2) de esta placa, y GND con GND.
  Monitor Serie a 115200 para ver que va pasando.
*/
#include <WiFi.h>
#include <HTTPClient.h>
#include "secrets.h"

const char* NODO_PASARELA = "pasarela-1";
const char* NODO_S3       = "nodo-s3";
const int   PIN_RX_S3     = 16;
const int   PIN_TX_S3     = 17;   // no se usa, solo se reserva
const unsigned long LATIDO_MS = 5000;
const int   CAMPOS_V      = 15;   // 14 datos de la ventana + espectro

char bufferLinea[400];
int  largoLinea = 0;
unsigned long contadorLatidos = 0, ultimoLatido = 0, ultimoDatoS3 = 0;
unsigned long recibidas = 0, subidas = 0, fallidas = 0;

// ------------------------------------------------------------------ WiFi
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
    Serial.printf(">>> WiFi OK. IP: %s  Canal: %d  Senal: %d dBm\n",
                  WiFi.localIP().toString().c_str(), WiFi.channel(), (int)WiFi.RSSI());
    Serial.printf(">>> Servidor: http://%s:%d\n", SERVIDOR_IP, SERVIDOR_PUERTO);
  } else {
    Serial.println(">>> No se pudo conectar al WiFi. Reintento en 5 s.");
  }
}

// ------------------------------------------------------------------ envio
int enviarJSON(const char* ruta, const char* json) {
  char url[96];
  snprintf(url, sizeof(url), "http://%s:%d%s", SERVIDOR_IP, SERVIDOR_PUERTO, ruta);
  HTTPClient http;
  http.setTimeout(3000);
  http.begin(url);
  http.addHeader("Content-Type", "application/json");
  int codigo = http.POST(json);
  if (codigo == 200) {
    subidas++;
  } else {
    fallidas++;
    if (codigo > 0) Serial.printf("   !! %s respondio %d: %s\n", ruta, codigo, http.getString().c_str());
    else            Serial.printf("   !! %s fallo: %s\n", ruta, http.errorToString(codigo).c_str());
  }
  http.end();
  return codigo;
}

// Parte la linea por comas (la modifica). Devuelve cuantos trozos hay.
int partir(char* linea, char* trozos[], int maximo) {
  int n = 0;
  char* p = linea;
  while (n < maximo) {
    trozos[n++] = p;
    char* coma = strchr(p, ',');
    if (!coma) break;
    *coma = 0;
    p = coma + 1;
  }
  return n;
}

bool esNumero(const char* s) {
  if (*s == '-') s++;
  if (!*s) return false;
  for (; *s; s++) if (!isdigit((unsigned char)*s)) return false;
  return true;
}

// ------------------------------------------------------------------ lineas del S3
void procesarVentana(char* resto) {
  char* t[CAMPOS_V];
  int n = partir(resto, t, CAMPOS_V);
  if (n != CAMPOS_V) { Serial.printf("   ?? ventana incompleta (%d campos), la ignoro\n", n); return; }
  for (int i = 0; i < 14; i++) if (!esNumero(t[i])) { Serial.println("   ?? ventana con datos raros, la ignoro"); return; }

  bool conEspectro = strlen(t[14]) == 126;
  static char json[700];
  int m = snprintf(json, sizeof(json),
    "{\"nodo\":\"%s\",\"ms\":%s,\"ventana\":%s,\"canal\":%s,\"tramas\":%s,\"aparatos\":%s,"
    "\"mgmt\":%s,\"datos\":%s,\"tam_prom\":%s,\"rssi_prom\":%s,\"rssi_max\":%s,"
    "\"tramas_debiles\":%s,\"aparatos_fuertes\":%s,\"tramas_dji\":%s,\"rssi_max_dji\":%s",
    NODO_S3, t[0], t[1], t[2], t[3], t[4], t[5], t[6], t[7], t[8], t[9], t[10], t[11], t[12], t[13]);
  if (conEspectro) snprintf(json + m, sizeof(json) - m, ",\"espectro\":\"%s\"}", t[14]);
  else             snprintf(json + m, sizeof(json) - m, "}");

  int codigo = enviarJSON("/api/ventanas", json);
  Serial.printf("Ventana #%s  %s tramas, %s aparatos, DJI %s, espectro %s -> %s\n",
                t[1], t[3], t[4], t[12], conEspectro ? "si" : "no",
                codigo == 200 ? "SUBIDA" : "FALLO");
}

void procesarLatidoS3(char* resto) {
  char* t[2];
  if (partir(resto, t, 2) != 2 || !esNumero(t[0]) || !esNumero(t[1])) return;
  char json[120];
  snprintf(json, sizeof(json), "{\"nodo\":\"%s\",\"contador\":%s,\"ms\":%s}", NODO_S3, t[0], t[1]);
  enviarJSON("/api/latido", json);
}

void procesarEvento(char* resto) {
  char* t[3];
  if (partir(resto, t, 3) != 3 || !esNumero(t[0])) return;
  char json[240];
  snprintf(json, sizeof(json), "{\"nodo\":\"%s\",\"ms\":%s,\"tipo\":\"%s\",\"detalle\":\"%s\"}",
           NODO_S3, t[0], t[1], t[2]);
  int codigo = enviarJSON("/api/eventos", json);
  Serial.printf(">>> EVENTO del S3: %s (%s) -> %s\n", t[1], t[2], codigo == 200 ? "SUBIDO" : "FALLO");
}

void procesarLinea(char* linea) {
  recibidas++;
  ultimoDatoS3 = millis();
  if (linea[0] == 0 || linea[1] != ',') return;   // notas u otras lineas
  switch (linea[0]) {
    case 'V': procesarVentana(linea + 2);  break;
    case 'L': procesarLatidoS3(linea + 2); break;
    case 'E': procesarEvento(linea + 2);   break;
  }
}

void leerCable() {
  while (Serial2.available()) {
    char c = Serial2.read();
    if (c == '\r') continue;
    if (c == '\n') {
      bufferLinea[largoLinea] = 0;
      if (largoLinea > 0) procesarLinea(bufferLinea);
      largoLinea = 0;
    } else if (largoLinea < (int)sizeof(bufferLinea) - 1) {
      bufferLinea[largoLinea++] = c;
    } else {
      largoLinea = 0;   // linea demasiado larga: se descarta
    }
  }
}

// ------------------------------------------------------------------ programa
void setup() {
  Serial.begin(115200);
  Serial2.setRxBufferSize(4096);   // por si el servidor tarda en responder
  Serial2.begin(115200, SERIAL_8N1, PIN_RX_S3, PIN_TX_S3);
  delay(500);
  Serial.println();
  Serial.println("=== devkit_pasarela: S3 (cable) -> WiFi -> servidor ===");
  conectarWiFi();
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    conectarWiFi();
    if (WiFi.status() != WL_CONNECTED) { delay(5000); return; }
  }
  leerCable();

  unsigned long ahora = millis();
  if (ahora - ultimoLatido >= LATIDO_MS) {
    ultimoLatido = ahora;
    char json[140];
    snprintf(json, sizeof(json), "{\"nodo\":\"%s\",\"contador\":%lu,\"ms\":%lu,\"rssi\":%d}",
             NODO_PASARELA, ++contadorLatidos, ahora, (int)WiFi.RSSI());
    enviarJSON("/api/latido", json);
    if (ultimoDatoS3 == 0 || ahora - ultimoDatoS3 > 10000)
      Serial.println("... no llega nada por el cable del S3 (revisa GPIO17 -> GPIO16 y GND)");
    else
      Serial.printf("(resumen: %lu lineas del S3, %lu subidas, %lu fallidas)\n", recibidas, subidas, fallidas);
  }
}
