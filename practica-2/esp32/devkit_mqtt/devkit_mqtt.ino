/*
  devkit_mqtt: Practica 2 (placa: ESP32 devkit, "ESP32 Dev Module")

  Igual que devkit_pasarela de la Practica 1, pero en vez de HTTP publica por MQTT
  en el broker de la nube, con usuario y clave.

    1. Se conecta al WiFi (datos en secrets.h).
    2. Se conecta al broker MQTT (Mosquitto en la VM de Azure) con usuario y clave.
       Deja un "testamento" (LWT): si se cae, el broker publica
       detector1/pasarela-1/estado = desconectado.
    3. Escucha el cable del S3 en GPIO 16 (RX2) y publica cada linea:
         V,...  -> detector1/nodo-s3/ventana      (JSON completo)
                   detector1/nodo-s3/nivel_banda  (numero: energia en 2423-2456 MHz)
                   detector1/nodo-s3/aparatos     (numero: aparatos WiFi vistos)
         L,...  -> detector1/nodo-s3/latido       (JSON)
         E,...  -> detector1/nodo-s3/evento       (JSON)
    4. Publica su propio latido cada 5 s en detector1/pasarela-1/latido.

  Libreria necesaria (Arduino IDE > Gestor de librerias): "PubSubClient" de Nick O'Leary.
  Cables: GPIO 17 del S3 -> GPIO 16 (RX2) de esta placa, y GND con GND.
  Monitor Serie a 115200.
*/
#include <WiFi.h>
#include <PubSubClient.h>
#include "secrets.h"

const char* NODO_PASARELA = "pasarela-1";
const char* NODO_S3       = "nodo-s3";
const char* RAIZ          = "detector1";     // como "invernadero1" en el enunciado
const int   PIN_RX_S3     = 16;
const int   PIN_TX_S3     = 17;              // no se usa, solo se reserva
const unsigned long LATIDO_MS = 5000;
const int   CAMPOS_V      = 15;              // 14 datos de la ventana + espectro
const int   BANDA_INI = 23, BANDA_FIN = 57;  // columnas 2423..2456 MHz

WiFiClient red;
PubSubClient mqtt(red);

char bufferLinea[400];
int  largoLinea = 0;
unsigned long contadorLatidos = 0, ultimoLatido = 0, ultimoDatoS3 = 0, ultimoIntentoMqtt = 0;
unsigned long recibidas = 0, publicadas = 0, fallidas = 0;
char idCliente[32];

// ------------------------------------------------------------------ WiFi y MQTT
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
  if (WiFi.status() == WL_CONNECTED)
    Serial.printf(">>> WiFi OK. IP: %s  Senal: %d dBm\n", WiFi.localIP().toString().c_str(), (int)WiFi.RSSI());
  else
    Serial.println(">>> No se pudo conectar al WiFi. Reintento en 5 s.");
}

void topic(char* destino, size_t n, const char* nodo, const char* hoja) {
  snprintf(destino, n, "%s/%s/%s", RAIZ, nodo, hoja);
}

bool conectarMqtt() {
  char tEstado[64];
  topic(tEstado, sizeof(tEstado), NODO_PASARELA, "estado");
  Serial.printf("Conectando al broker %s:%d como \"%s\"... ", MQTT_SERVIDOR, MQTT_PUERTO, MQTT_USUARIO);
  // LWT: si la placa se cae, el broker avisa por ella (retenido)
  bool ok = mqtt.connect(idCliente, MQTT_USUARIO, MQTT_CLAVE, tEstado, 1, true, "desconectado");
  if (ok) {
    mqtt.publish(tEstado, "conectado", true);
    Serial.println("OK");
  } else {
    // -2 = no llega al servidor (IP, puerto o grupo de seguridad); 4/5 = usuario o clave
    Serial.printf("fallo (estado %d)\n", mqtt.state());
  }
  return ok;
}

bool publicar(const char* nodo, const char* hoja, const char* texto, bool retener = false) {
  char t[64];
  topic(t, sizeof(t), nodo, hoja);
  bool ok = mqtt.connected() && mqtt.publish(t, texto, retener);
  if (ok) publicadas++; else fallidas++;
  return ok;
}

// ------------------------------------------------------------------ utilidades
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

// Promedio 0..8 de la banda del dron dentro del espectro de 126 digitos
float nivelBanda(const char* espectro) {
  int suma = 0;
  for (int i = BANDA_INI; i < BANDA_FIN; i++) suma += espectro[i] - '0';
  return (float)suma / (BANDA_FIN - BANDA_INI);
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
    "{\"ms\":%s,\"ventana\":%s,\"canal\":%s,\"tramas\":%s,\"aparatos\":%s,"
    "\"mgmt\":%s,\"datos\":%s,\"tam_prom\":%s,\"rssi_prom\":%s,\"rssi_max\":%s,"
    "\"tramas_debiles\":%s,\"aparatos_fuertes\":%s,\"tramas_dji\":%s,\"rssi_max_dji\":%s",
    t[0], t[1], t[2], t[3], t[4], t[5], t[6], t[7], t[8], t[9], t[10], t[11], t[12], t[13]);
  if (conEspectro) snprintf(json + m, sizeof(json) - m, ",\"espectro\":\"%s\"}", t[14]);
  else             snprintf(json + m, sizeof(json) - m, "}");

  bool ok = publicar(NODO_S3, "ventana", json);
  publicar(NODO_S3, "aparatos", t[4]);
  char nivel[12] = "-";
  if (conEspectro) {
    snprintf(nivel, sizeof(nivel), "%.2f", nivelBanda(t[14]));
    publicar(NODO_S3, "nivel_banda", nivel);
  }
  Serial.printf("Ventana #%s  %s tramas, %s aparatos, banda %s -> %s\n",
                t[1], t[3], t[4], nivel, ok ? "PUBLICADA" : "FALLO");
}

void procesarLatidoS3(char* resto) {
  char* t[2];
  if (partir(resto, t, 2) != 2 || !esNumero(t[0]) || !esNumero(t[1])) return;
  char json[80];
  snprintf(json, sizeof(json), "{\"contador\":%s,\"ms\":%s}", t[0], t[1]);
  publicar(NODO_S3, "latido", json);
}

void procesarEvento(char* resto) {
  char* t[3];
  if (partir(resto, t, 3) != 3 || !esNumero(t[0])) return;
  char json[240];
  snprintf(json, sizeof(json), "{\"ms\":%s,\"tipo\":\"%s\",\"detalle\":\"%s\"}", t[0], t[1], t[2]);
  bool ok = publicar(NODO_S3, "evento", json);
  Serial.printf(">>> EVENTO del S3: %s (%s) -> %s\n", t[1], t[2], ok ? "PUBLICADO" : "FALLO");
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
  Serial2.setRxBufferSize(4096);
  Serial2.begin(115200, SERIAL_8N1, PIN_RX_S3, PIN_TX_S3);
  delay(500);
  Serial.println();
  Serial.println("=== devkit_mqtt: S3 (cable) -> WiFi -> broker MQTT ===");

  uint8_t mac[6];
  WiFi.macAddress(mac);
  snprintf(idCliente, sizeof(idCliente), "devkit-%02x%02x%02x", mac[3], mac[4], mac[5]);

  mqtt.setServer(MQTT_SERVIDOR, MQTT_PUERTO);
  mqtt.setBufferSize(1024);   // la ventana con espectro pesa ~600 bytes (el valor por defecto es 256)
  mqtt.setKeepAlive(30);
  conectarWiFi();
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    conectarWiFi();
    if (WiFi.status() != WL_CONNECTED) { delay(5000); return; }
  }
  unsigned long ahora = millis();
  if (!mqtt.connected() && ahora - ultimoIntentoMqtt >= 5000) {
    ultimoIntentoMqtt = ahora;
    conectarMqtt();
  }
  mqtt.loop();
  leerCable();

  if (ahora - ultimoLatido >= LATIDO_MS) {
    ultimoLatido = ahora;
    char json[100];
    snprintf(json, sizeof(json), "{\"contador\":%lu,\"ms\":%lu,\"rssi\":%d}",
             ++contadorLatidos, ahora, (int)WiFi.RSSI());
    publicar(NODO_PASARELA, "latido", json);
    if (ultimoDatoS3 == 0 || ahora - ultimoDatoS3 > 10000)
      Serial.println("... no llega nada por el cable del S3 (revisa GPIO17 -> GPIO16 y GND)");
    else
      Serial.printf("(resumen: %lu lineas del S3, %lu publicadas, %lu fallidas, MQTT %s)\n",
                    recibidas, publicadas, fallidas, mqtt.connected() ? "conectado" : "desconectado");
  }
}
