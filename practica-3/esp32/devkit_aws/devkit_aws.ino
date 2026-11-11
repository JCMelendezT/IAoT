/*
  devkit_aws — Practica 3 (placa: ESP32 devkit, "ESP32 Dev Module")

  Misma idea de la Practica 2, pero hablando con AWS IoT Core (MQTT sobre TLS, puerto 8883,
  con certificado de la placa en vez de usuario y clave).

    1. Se conecta al WiFi (secrets.h) y pone la hora por internet (NTP; TLS la necesita).
    2. Se conecta a AWS IoT Core con el certificado de certs.h (lo genera Terraform).
    3. Escucha el cable del S3 en GPIO 16 (RX2). Cada ventana "V,..." calcula
       nivel_banda (energia en 2423-2456 MHz) y cada 5 s publica una lectura:
         detector1/nodo-s3/lectura   {"nivel_banda":..,"aparatos":..,"tramas":..,"rssi_max":..,"alerta":0|1}
       Las lineas "E,..." del S3 se publican en detector1/nodo-s3/evento.
       Con nodo_detector (IA en el S3) llega ademas "D,ms,clase,p_fondo,p_bt,p_dron,alerta":
       la lectura lleva "clase" y "prob_dron", y "alerta" pasa a ser la del modelo.
    4. Actuador (LED en GPIO 2, o un rele/buzzer): escucha detector1/actuador/cmd
       {"estado":"on|off"} y responde en detector1/actuador/estado.

  Librerias (Arduino IDE > Gestor de librerias): "PubSubClient" (Nick O'Leary).
  Cables: GPIO 17 del S3 -> GPIO 16 (RX2) de esta placa, y GND con GND.
  Monitor Serie a 115200.
*/
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <time.h>
#include "secrets.h"   // WIFI_NOMBRE y WIFI_CLAVE
#include "certs.h"     // AWS_IOT_ENDPOINT, AWS_THING_NAME y certificados (generado por Terraform)

const char* RAIZ      = "detector1";
const char* NODO_S3   = "nodo-s3";
const int   PIN_RX_S3 = 16;
const int   PIN_TX_S3 = 17;                 // no se usa, solo se reserva
const int   PIN_ACTUADOR = 2;               // LED integrado del devkit
const unsigned long LECTURA_MS = 5000;      // una lectura cada 5 s hacia AWS
const int   CAMPOS_V  = 15;
const int   BANDA_INI = 23, BANDA_FIN = 57; // columnas 2423..2456 MHz
const float UMBRAL_ALERTA = 2.5;            // solo si el S3 no manda la decision del modelo
const unsigned long MODELO_VIGENTE_MS = 10000;

WiFiClientSecure red;
PubSubClient mqtt(red);

char bufferLinea[400];
int  largoLinea = 0;
unsigned long ultimaLectura = 0, ultimoIntentoMqtt = 0, ultimoDatoS3 = 0;
unsigned long recibidas = 0, publicadas = 0, fallidas = 0;
bool actuadorEncendido = false;

// Ultima ventana vista (se publica cada LECTURA_MS)
bool  hayVentana = false;
float uNivel = 0;
int   uAparatos = 0, uTramas = 0, uRssiMax = 0, uVentana = 0, uCanal = 0;

// Ultima decision del modelo del S3 (lineas D)
char  uClase[16] = "";
float uProbDron = 0;
int   uAlertaModelo = 0;
unsigned long ultimoModelo = 0;

char tCmd[48], tEstado[48], tLectura[48], tEvento[48];

// ------------------------------------------------------------------ WiFi, hora y MQTT
void conectarWiFi() {
  Serial.printf("Conectando a \"%s\"", WIFI_NOMBRE);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_NOMBRE, WIFI_CLAVE);
  unsigned long inicio = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - inicio < 20000) { delay(500); Serial.print("."); }
  Serial.println();
  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf(">>> WiFi OK. IP: %s  Senal: %d dBm\n", WiFi.localIP().toString().c_str(), (int)WiFi.RSSI());
    configTime(0, 0, "pool.ntp.org", "time.google.com");   // TLS valida las fechas del certificado
    Serial.print("Poniendo la hora");
    time_t ahora = time(nullptr);
    int intentos = 0;
    while (ahora < 1700000000 && intentos++ < 40) { delay(500); Serial.print("."); ahora = time(nullptr); }
    Serial.println(ahora < 1700000000 ? " fallo (sin hora, TLS no va a conectar)" : " OK");
  } else {
    Serial.println(">>> No se pudo conectar al WiFi. Reintento en 5 s.");
  }
}

void publicarEstado() {
  char json[64];
  snprintf(json, sizeof(json), "{\"estado\":\"%s\"}", actuadorEncendido ? "on" : "off");
  mqtt.publish(tEstado, json);
}

void aplicarActuador(bool encender) {
  actuadorEncendido = encender;
  digitalWrite(PIN_ACTUADOR, encender ? HIGH : LOW);
  Serial.printf(">>> ACTUADOR %s\n", encender ? "ENCENDIDO" : "APAGADO");
  publicarEstado();
}

void alRecibir(char* topic, byte* carga, unsigned int largo) {
  char msg[160];
  unsigned int n = largo < sizeof(msg) - 1 ? largo : sizeof(msg) - 1;
  memcpy(msg, carga, n);
  msg[n] = 0;
  Serial.printf("<<< %s : %s\n", topic, msg);
  if (strcmp(topic, tCmd) != 0) return;
  if (strstr(msg, "\"on\""))       aplicarActuador(true);
  else if (strstr(msg, "\"off\"")) aplicarActuador(false);
}

bool conectarMqtt() {
  Serial.printf("Conectando a AWS IoT (%s) como \"%s\"... ", AWS_IOT_ENDPOINT, AWS_THING_NAME);
  bool ok = mqtt.connect(AWS_THING_NAME);
  if (ok) {
    mqtt.subscribe(tCmd, 1);
    publicarEstado();
    Serial.println("OK (escuchando comandos)");
  } else {
    // -2 = no llega (WiFi/hora/endpoint); 5 = no autorizado (certificado o politica)
    Serial.printf("fallo (estado %d)\n", mqtt.state());
  }
  return ok;
}

bool publicar(const char* topic, const char* texto) {
  bool ok = mqtt.connected() && mqtt.publish(topic, texto);
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

float nivelBanda(const char* espectro) {
  int suma = 0;
  for (int i = BANDA_INI; i < BANDA_FIN; i++) suma += espectro[i] - '0';
  return (float)suma / (BANDA_FIN - BANDA_INI);
}

// ------------------------------------------------------------------ lineas del S3
void procesarVentana(char* resto) {
  char* t[CAMPOS_V];
  if (partir(resto, t, CAMPOS_V) != CAMPOS_V) return;
  for (int i = 0; i < 14; i++) if (!esNumero(t[i])) return;
  if (strlen(t[14]) != 126) return;
  uVentana  = atoi(t[1]);
  uCanal    = atoi(t[2]);
  uTramas   = atoi(t[3]);
  uAparatos = atoi(t[4]);
  uRssiMax  = atoi(t[9]);
  uNivel    = nivelBanda(t[14]);
  hayVentana = true;
}

void procesarEvento(char* resto) {
  char* t[3];
  if (partir(resto, t, 3) != 3 || !esNumero(t[0])) return;
  char json[200];
  snprintf(json, sizeof(json), "{\"tipo\":\"%s\",\"detalle\":\"%s\",\"ms\":%s}", t[1], t[2], t[0]);
  bool ok = publicar(tEvento, json);
  Serial.printf(">>> EVENTO del S3: %s (%s) -> %s\n", t[1], t[2], ok ? "PUBLICADO" : "FALLO");
}

void procesarModelo(char* resto) {
  char* t[6];
  if (partir(resto, t, 6) != 6 || !esNumero(t[0]) || !esNumero(t[5])) return;
  strncpy(uClase, t[1], sizeof(uClase) - 1);
  uClase[sizeof(uClase) - 1] = 0;
  uProbDron = atof(t[4]);
  uAlertaModelo = atoi(t[5]);
  ultimoModelo = millis();
}

void procesarLinea(char* linea) {
  ultimoDatoS3 = millis();
  recibidas++;
  if (linea[0] == 0 || linea[1] != ',') return;
  if (linea[0] == 'V') procesarVentana(linea + 2);
  else if (linea[0] == 'E') procesarEvento(linea + 2);
  else if (linea[0] == 'D') procesarModelo(linea + 2);
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
      largoLinea = 0;
    }
  }
}

void publicarLectura() {
  if (!hayVentana) return;
  bool conModelo = ultimoModelo != 0 && millis() - ultimoModelo < MODELO_VIGENTE_MS;
  int alerta = conModelo ? uAlertaModelo : (uNivel > UMBRAL_ALERTA ? 1 : 0);
  char json[320];
  int m = snprintf(json, sizeof(json),
    "{\"nivel_banda\":%.2f,\"aparatos\":%d,\"tramas\":%d,\"rssi_max\":%d,\"ventana\":%d,\"canal\":%d,\"alerta\":%d,\"wifi_rssi\":%d",
    uNivel, uAparatos, uTramas, uRssiMax, uVentana, uCanal, alerta, (int)WiFi.RSSI());
  if (conModelo)
    m += snprintf(json + m, sizeof(json) - m, ",\"clase\":\"%s\",\"prob_dron\":%.3f", uClase, uProbDron);
  snprintf(json + m, sizeof(json) - m, "}");
  bool ok = publicar(tLectura, json);
  if (conModelo)
    Serial.printf("Lectura: IA dice %s (dron %d %%), %d aparatos%s -> %s\n", uClase, (int)lroundf(uProbDron * 100),
                  uAparatos, alerta ? "  [ALERTA]" : "", ok ? "PUBLICADA" : "FALLO");
  else
    Serial.printf("Lectura: banda %.2f, %d aparatos%s -> %s\n", uNivel, uAparatos,
                  alerta ? "  [ALERTA]" : "", ok ? "PUBLICADA" : "FALLO");
  hayVentana = false;
}

// ------------------------------------------------------------------ programa
void setup() {
  Serial.begin(115200);
  Serial2.setRxBufferSize(4096);
  Serial2.begin(115200, SERIAL_8N1, PIN_RX_S3, PIN_TX_S3);
  pinMode(PIN_ACTUADOR, OUTPUT);
  digitalWrite(PIN_ACTUADOR, LOW);
  delay(500);
  Serial.println();
  Serial.println("=== devkit_aws: S3 (cable) -> WiFi -> AWS IoT Core (TLS) ===");

  snprintf(tCmd,     sizeof(tCmd),     "%s/actuador/cmd", RAIZ);
  snprintf(tEstado,  sizeof(tEstado),  "%s/actuador/estado", RAIZ);
  snprintf(tLectura, sizeof(tLectura), "%s/%s/lectura", RAIZ, NODO_S3);
  snprintf(tEvento,  sizeof(tEvento),  "%s/%s/evento", RAIZ, NODO_S3);

  red.setCACert(AWS_CERT_CA);
  red.setCertificate(AWS_CERT_CRT);
  red.setPrivateKey(AWS_CERT_KEY);
  mqtt.setServer(AWS_IOT_ENDPOINT, 8883);
  mqtt.setCallback(alRecibir);
  mqtt.setBufferSize(512);
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

  if (ahora - ultimaLectura >= LECTURA_MS) {
    ultimaLectura = ahora;
    publicarLectura();
    if (ultimoDatoS3 == 0 || ahora - ultimoDatoS3 > 10000)
      Serial.println("... no llega nada por el cable del S3 (revisa GPIO17 -> GPIO16 y GND)");
  }
}
