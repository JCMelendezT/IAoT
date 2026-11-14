/*
  devkit_centinela: pasarela del detector con app para el celular (placa: ESP32 devkit)

  Que hace:
    1. Recibe por el cable lo que manda el S3 (nodo_detector): cada ventana con sus rasgos,
       la decision del modelo y las alertas.
    2. Lo guarda en su memoria (caja negra, ~9-12 h) para etiquetarlo y reentrenar.
    3. Sirve la app del celular en http://detector.local (o http://192.168.4.1 si no hay WiFi).
       Desde la app: ver en vivo, grabar sesiones etiquetadas, etiquetar lo grabado,
       reentrenar el modelo en el celular y mandarlo al S3, y cambiar ajustes.
    4. Si USAR_AWS es 1 y el Learner Lab esta prendido, sigue subiendo las lecturas a AWS IoT
       (Practica 3), igual que devkit_aws.

  Arduino IDE:
    - Placa "ESP32 Dev Module". Partition Scheme: "No OTA (2MB APP/2MB SPIFFS)" (recomendado:
      mas espacio para datos). Con la de fabrica tambien funciona.
    - Librerias: "PubSubClient" (solo si USAR_AWS es 1).
    - secrets.h con WIFI_NOMBRE y WIFI_CLAVE (copia secrets.h.example). certs.h lo genera
      Terraform (o copialo de practica-3/esp32/devkit_aws/).

  Cables con el S3 (GND con GND):
    devkit GPIO 16 (RX2) <- GPIO 17 del S3   (datos del detector)
    devkit GPIO 17 (TX2) -> GPIO 18 del S3   (ordenes de la app: modelo y ajustes)
*/
#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <Preferences.h>
#include <vector>
#include <time.h>
#include <sys/time.h>
#include "secrets.h"
#include "almacen.h"
#include "app_web.h"      // la app comprimida; la genera app/empaquetar.py

#define USAR_AWS 1
#if USAR_AWS
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include "certs.h"
#endif

#ifndef AP_NOMBRE
#define AP_NOMBRE "CENTINELA-RF"
#endif
#ifndef AP_CLAVE
#define AP_CLAVE "centinela2026"
#endif

const int PIN_RX_S3 = 16;
const int PIN_TX_S3 = 17;
const char* NOMBRE_MDNS = "detector";          // http://detector.local

WebServer web(80);
bool modoAP = false;                           // sin WiFi: solo la red propia CENTINELA-RF

struct RedWiFi { String nombre, clave; bool fija; };   // fija = viene de secrets.h
const int MAX_REDES_APP = 5;
std::vector<RedWiFi> redesConocidas;
bool apAbierta = false;                        // la red propia esta encendida
unsigned long apDesde = 0, caidoDesde = 0;
bool pruebaPendiente = false;
String pruebaNombre, resultadoPrueba;          // "", "probando", "ok" o "fallo"

String sinComillas(String t) { t.replace("\\", ""); t.replace("\"", ""); return t; }

// ------------------------------------------------------------------ estado en vivo
struct EstadoS3 {
  unsigned long ultimoDato = 0;   // millis del devkit
  uint32_t ventana = 0;
  int clase = -1;
  float prob[3] = {0, 0, 0};
  int alerta = 0;
  uint32_t alertas = 0;
  // ficha (linea I)
  bool hayFicha = false;
  uint32_t version = 0;
  float exactitud = 0;
  int nRasgos = 0, ventanasAlerta = 2, ventanasApagar = 2, voz = 1, repetir = 5;
} s3;

struct Evento { uint32_t epoch; char texto[48]; };
Evento eventos[10];
int nEventos = 0;

// Grabacion etiquetada en curso
uint8_t grabEtiqueta = 0;
uint32_t grabFin = 0;          // epoch en que termina

// Respuesta del S3 a una orden
bool hayRespuesta = false;
bool respuestaOk = false;
char respuestaTexto[80] = "";

// Lectura para AWS (como devkit_aws)
bool hayVentanaV = false;
float uNivel = 0;
int uAparatos = 0, uTramas = 0, uRssiMax = 0, uVentanaV = 0;

// ------------------------------------------------------------------ hora
bool horaValida() { return time(nullptr) > 1700000000; }
uint32_t ahoraEpoch() { return horaValida() ? (uint32_t)time(nullptr) : 0; }

void agregarEvento(const char* texto) {
  if (nEventos == 10) { memmove(&eventos[0], &eventos[1], sizeof(Evento) * 9); nEventos = 9; }
  eventos[nEventos].epoch = ahoraEpoch();
  strncpy(eventos[nEventos].texto, texto, sizeof(eventos[0].texto) - 1);
  eventos[nEventos].texto[sizeof(eventos[0].texto) - 1] = 0;
  nEventos++;
}

// ------------------------------------------------------------------ AWS (opcional)
#if USAR_AWS
WiFiClientSecure red;
PubSubClient mqtt(red);
unsigned long ultimoIntentoMqtt = 0, ultimaLecturaAws = 0;
unsigned long esperaMqtt = 5000;
const char* T_LECTURA = "detector1/nodo-s3/lectura";
const char* T_EVENTO  = "detector1/nodo-s3/evento";
const char* T_CMD     = "detector1/actuador/cmd";
const char* T_ESTADO  = "detector1/actuador/estado";

void alRecibirMqtt(char* topic, byte* carga, unsigned int largo) {
  if (strcmp(topic, T_CMD) != 0) return;
  bool on = false;
  for (unsigned int i = 0; i + 3 < largo; i++) if (!memcmp(carga + i, "\"on\"", 4)) on = true;
  digitalWrite(2, on ? HIGH : LOW);
  mqtt.publish(T_ESTADO, on ? "{\"estado\":\"on\"}" : "{\"estado\":\"off\"}");
}

void awsIniciar() {
  red.setCACert(AWS_CERT_CA);
  red.setCertificate(AWS_CERT_CRT);
  red.setPrivateKey(AWS_CERT_KEY);
  red.setTimeout(5);
  mqtt.setServer(AWS_IOT_ENDPOINT, 8883);
  mqtt.setCallback(alRecibirMqtt);
  mqtt.setBufferSize(512);
  mqtt.setKeepAlive(30);
  pinMode(2, OUTPUT);
}

void awsAtender() {
  if (modoAP || !horaValida()) return;
  unsigned long ahora = millis();
  if (!mqtt.connected() && ahora - ultimoIntentoMqtt >= esperaMqtt) {
    ultimoIntentoMqtt = ahora;
    Serial.print("AWS IoT... ");
    if (mqtt.connect(AWS_THING_NAME)) {
      mqtt.subscribe(T_CMD, 1);
      Serial.println("conectado");
      esperaMqtt = 5000;
    } else {
      Serial.printf("no conecta (estado %d). Reintento en %lu s\n", mqtt.state(), esperaMqtt / 1000);
      esperaMqtt = min(esperaMqtt * 2, 120000UL);   // si el Learner Lab esta apagado, espera mas
    }
  }
  mqtt.loop();
  if (mqtt.connected() && hayVentanaV && ahora - ultimaLecturaAws >= 5000) {
    ultimaLecturaAws = ahora;
    char json[320];
    bool conModelo = s3.clase >= 0 && ahora - s3.ultimoDato < 10000;
    int m = snprintf(json, sizeof(json),
      "{\"nivel_banda\":%.2f,\"aparatos\":%d,\"tramas\":%d,\"rssi_max\":%d,\"ventana\":%d,\"canal\":0,\"alerta\":%d,\"wifi_rssi\":%d",
      uNivel, uAparatos, uTramas, uRssiMax, uVentanaV, conModelo ? s3.alerta : 0, (int)WiFi.RSSI());
    if (conModelo)
      m += snprintf(json + m, sizeof(json) - m, ",\"clase\":\"%s\",\"prob_dron\":%.3f",
                    s3.clase == 2 ? "dron" : (s3.clase == 1 ? "bluetooth" : "fondo"), s3.prob[2]);
    snprintf(json + m, sizeof(json) - m, "}");
    mqtt.publish(T_LECTURA, json);
    hayVentanaV = false;
  }
}

void awsEvento(const char* tipo, const char* detalle, const char* ms) {
  if (!mqtt.connected()) return;
  char json[200];
  snprintf(json, sizeof(json), "{\"tipo\":\"%s\",\"detalle\":\"%s\",\"ms\":%s}", tipo, detalle, ms);
  mqtt.publish(T_EVENTO, json);
}
#endif

// ------------------------------------------------------------------ lineas del S3
int partir(char* linea, char* t[], int maximo) {
  int n = 0;
  char* p = linea;
  while (n < maximo) {
    t[n++] = p;
    char* coma = strchr(p, ',');
    if (!coma) break;
    *coma = 0;
    p = coma + 1;
  }
  return n;
}

void procesarR(char* resto) {          // R,ms,ventana,clase,p0,p1,p2,alerta,<35 rasgos>
  char* t[7 + N_RASGOS];
  if (partir(resto, t, 7 + N_RASGOS) != 7 + N_RASGOS) return;
  s3.ventana = strtoul(t[1], NULL, 10);
  s3.clase = atoi(t[2]);
  for (int k = 0; k < 3; k++) s3.prob[k] = atof(t[3 + k]);
  s3.alerta = atoi(t[6]);

  Registro reg;
  reg.epoch = ahoraEpoch();
  reg.ventana = s3.ventana;
  reg.clase = (uint8_t)s3.clase;
  reg.pDron = (uint8_t)lroundf(constrain(s3.prob[2], 0.0f, 1.0f) * 255);
  reg.alerta = (uint8_t)s3.alerta;
  if (grabEtiqueta && reg.epoch && reg.epoch >= grabFin) grabEtiqueta = 0;   // termino la grabacion
  reg.etiqueta = grabEtiqueta;
  for (int i = 0; i < N_RASGOS; i++) reg.r[i] = escalar(i, atof(t[7 + i]));
  almacenGuardar(reg);
}

void procesarV(char* resto) {          // para la lectura de AWS (nivel de la banda del dron)
  char* t[15];
  if (partir(resto, t, 15) != 15 || strlen(t[14]) != 126) return;
  uVentanaV = atoi(t[1]);
  uTramas = atoi(t[3]);
  uAparatos = atoi(t[4]);
  uRssiMax = atoi(t[9]);
  int suma = 0;
  for (int i = 23; i < 57; i++) suma += t[14][i] - '0';
  uNivel = suma / 34.0f;
  hayVentanaV = true;
}

void procesarE(char* resto) {          // E,ms,tipo,detalle
  char* t[3];
  if (partir(resto, t, 3) != 3) return;
  char texto[48];
  snprintf(texto, sizeof(texto), "%s %s", t[1], t[2]);
  agregarEvento(texto);
  if (!strcmp(t[1], "ALERTA_DRON")) s3.alertas++;
#if USAR_AWS
  awsEvento(t[1], t[2], t[0]);
#endif
}

void procesarI(char* resto) {          // I,version,exactitud,n,alerta,apagar,voz,repetir
  char* t[7];
  if (partir(resto, t, 7) != 7) return;
  s3.hayFicha = true;
  s3.version = strtoul(t[0], NULL, 10);
  s3.exactitud = atof(t[1]);
  s3.nRasgos = atoi(t[2]);
  s3.ventanasAlerta = atoi(t[3]);
  s3.ventanasApagar = atoi(t[4]);
  s3.voz = atoi(t[5]);
  s3.repetir = atoi(t[6]);
}

void procesarA(char* resto) {          // A,OK|ERROR,detalle
  char* t[2];
  if (partir(resto, t, 2) != 2) return;
  respuestaOk = !strcmp(t[0], "OK");
  strncpy(respuestaTexto, t[1], sizeof(respuestaTexto) - 1);
  respuestaTexto[sizeof(respuestaTexto) - 1] = 0;
  hayRespuesta = true;
}

void procesarLinea(char* linea) {
  if (!linea[0] || linea[1] != ',') return;
  s3.ultimoDato = millis();
  switch (linea[0]) {
    case 'R': procesarR(linea + 2); break;
    case 'V': procesarV(linea + 2); break;
    case 'E': procesarE(linea + 2); break;
    case 'I': procesarI(linea + 2); break;
    case 'A': procesarA(linea + 2); break;
    default: break;   // D y otras: la informacion ya viene en R
  }
}

void leerCable() {
  static char buf[800];
  static int largo = 0;
  while (Serial2.available()) {
    char c = Serial2.read();
    if (c == '\r') continue;
    if (c == '\n') {
      buf[largo] = 0;
      if (largo > 0) procesarLinea(buf);
      largo = 0;
    } else if (largo < (int)sizeof(buf) - 1) {
      buf[largo++] = c;
    } else {
      largo = 0;
    }
  }
}

// Manda una orden al S3 y espera su respuesta (A,...) hasta 4 s
bool ordenS3(const String& orden, String& detalle) {
  hayRespuesta = false;
  Serial2.print(orden);
  Serial2.print('\n');
  unsigned long inicio = millis();
  while (millis() - inicio < 4000) {
    leerCable();
    if (hayRespuesta) { detalle = respuestaTexto; return respuestaOk; }
    delay(5);
  }
  detalle = "el S3 no respondio (revisa el cable GPIO 17 del devkit -> GPIO 18 del S3)";
  return false;
}

// ------------------------------------------------------------------ app y API
void responderJson(int codigo, const String& json) {
  web.sendHeader("Cache-Control", "no-store");
  web.send(codigo, "application/json", json);
}

void apiApp() {
  web.sendHeader("Content-Encoding", "gzip");
  web.sendHeader("Cache-Control", "no-cache");
  web.send_P(200, "text/html", (const char*)APP_HTML_GZ, APP_HTML_GZ_LEN);
}

void apiEstado() {
  uint32_t ahora = ahoraEpoch();
  bool vivo = s3.ultimoDato && millis() - s3.ultimoDato < 8000;
  String j;
  j.reserve(1400);
  j += "{\"epoch\":"; j += ahora;
  j += ",\"uptime\":"; j += millis() / 1000;
  j += ",\"wifi\":{\"modo\":\""; j += modoAP ? "ap" : "red";
  j += "\",\"red\":\""; j += sinComillas(modoAP ? String(AP_NOMBRE) : WiFi.SSID());
  j += "\",\"prueba\":\""; j += resultadoPrueba;
  j += "\",\"prueba_red\":\""; j += sinComillas(pruebaNombre);
  j += "\",\"ip\":\""; j += modoAP ? WiFi.softAPIP().toString() : WiFi.localIP().toString();
  j += "\",\"rssi\":"; j += modoAP ? 0 : WiFi.RSSI(); j += "}";
  j += ",\"s3\":{\"vivo\":"; j += vivo ? "true" : "false";
  j += ",\"ventana\":"; j += s3.ventana;
  j += ",\"clase\":"; j += s3.clase;
  j += ",\"prob\":["; j += String(s3.prob[0], 3); j += ","; j += String(s3.prob[1], 3); j += ","; j += String(s3.prob[2], 3); j += "]";
  j += ",\"alerta\":"; j += s3.alerta;
  j += ",\"alertas\":"; j += s3.alertas; j += "}";
  j += ",\"modelo\":{\"conocido\":"; j += s3.hayFicha ? "true" : "false";
  j += ",\"version\":"; j += s3.version;
  j += ",\"exactitud\":"; j += String(s3.exactitud, 4);
  j += ",\"rasgos\":"; j += s3.nRasgos; j += "}";
  j += ",\"ajustes\":{\"alerta\":"; j += s3.ventanasAlerta;
  j += ",\"apagar\":"; j += s3.ventanasApagar;
  j += ",\"voz\":"; j += s3.voz;
  j += ",\"repetir\":"; j += s3.repetir; j += "}";
  j += ",\"grabacion\":{\"etiqueta\":\""; j += NOMBRES_ETIQUETA[grabEtiqueta];
  j += "\",\"restante\":"; j += (grabEtiqueta && grabFin > ahora) ? (grabFin - ahora) : 0; j += "}";
  j += ",\"almacen\":{\"registros\":"; j += almacenRegistros();
  j += ",\"horas_max\":"; j += String(2.0f * maxBytesArchivo / sizeof(Registro) * 2 / 3600, 1); j += "}";
#if USAR_AWS
  j += ",\"aws\":{\"conectado\":"; j += mqtt.connected() ? "true" : "false"; j += "}";
#endif
  j += ",\"eventos\":[";
  for (int i = nEventos - 1; i >= 0; i--) {
    j += "{\"epoch\":"; j += eventos[i].epoch; j += ",\"texto\":\""; j += eventos[i].texto; j += "\"}";
    if (i) j += ",";
  }
  j += "]}";
  responderJson(200, j);
}

void apiHora() {                       // el celular le pasa la hora (util en modo AP)
  uint32_t epoch = web.arg("epoch").toInt();
  if (epoch > 1700000000 && !horaValida()) {
    struct timeval tv = {(time_t)epoch, 0};
    settimeofday(&tv, NULL);
  }
  responderJson(200, String("{\"epoch\":") + ahoraEpoch() + "}");
}

int etiquetaDesdeTexto(const String& s) {
  for (int k = 1; k < 4; k++) if (s == NOMBRES_ETIQUETA[k]) return k;
  return 0;
}

void apiGrabar() {
  int et = etiquetaDesdeTexto(web.arg("clase"));
  uint32_t minutos = web.arg("minutos").toInt();
  if (!horaValida()) { responderJson(409, "{\"error\":\"el detector aun no tiene la hora; recarga la app\"}"); return; }
  if (et == 0 || minutos == 0) { grabEtiqueta = 0; grabFin = 0; }
  else { grabEtiqueta = et; grabFin = ahoraEpoch() + minutos * 60; }
  agregarEvento(grabEtiqueta ? "GRABACION inicio" : "GRABACION fin");
  responderJson(200, "{\"ok\":true}");
}

void apiEtiquetar() {
  uint32_t desde = web.arg("desde").toInt(), hasta = web.arg("hasta").toInt();
  String clase = web.arg("clase");
  int et = clase == "ninguna" ? 0 : etiquetaDesdeTexto(clase);
  if (!desde || hasta < desde || (et == 0 && clase != "ninguna")) { responderJson(400, "{\"error\":\"rango o clase invalidos\"}"); return; }
  uint32_t n = almacenEtiquetar(desde, hasta, et);
  responderJson(200, String("{\"cambiados\":") + n + "}");
}

// Resumen por minuto para la linea de tiempo de la app
void apiHistorial() {
  int horas = constrain(web.arg("horas").toInt(), 1, 24);
  uint32_t ahora = ahoraEpoch();
  if (!ahora) { responderJson(409, "{\"error\":\"sin hora\"}"); return; }
  uint32_t inicioMin = ahora / 60 - horas * 60 + 1;
  int n = horas * 60;
  uint16_t* cuenta = (uint16_t*)calloc(n, sizeof(uint16_t));
  uint16_t* dron = (uint16_t*)calloc(n, sizeof(uint16_t));
  uint16_t* alerta = (uint16_t*)calloc(n, sizeof(uint16_t));
  uint8_t* maxp = (uint8_t*)calloc(n, 1);
  uint8_t* etiq = (uint8_t*)calloc(n, 1);
  if (!cuenta || !dron || !alerta || !maxp || !etiq) {
    free(cuenta); free(dron); free(alerta); free(maxp); free(etiq);
    responderJson(500, "{\"error\":\"sin memoria\"}");
    return;
  }
  almacenRecorrer([&](const Registro& r) {
    if (!r.epoch) return true;
    int32_t i = (int32_t)(r.epoch / 60) - (int32_t)inicioMin;
    if (i < 0 || i >= n) return true;
    cuenta[i]++;
    if (r.clase == 2) dron[i]++;
    if (r.alerta) alerta[i]++;
    if (r.pDron > maxp[i]) maxp[i] = r.pDron;
    if (r.etiqueta) etiq[i] = r.etiqueta;
    return true;
  });
  web.setContentLength(CONTENT_LENGTH_UNKNOWN);
  web.send(200, "application/json", "");
  String trozo = String("{\"inicio\":") + (inicioMin * 60) + ",\"min\":[";
  for (int i = 0; i < n; i++) {
    if (i) trozo += ",";
    trozo += "["; trozo += cuenta[i]; trozo += ","; trozo += dron[i]; trozo += ","; trozo += alerta[i];
    trozo += ","; trozo += maxp[i]; trozo += ","; trozo += etiq[i]; trozo += "]";
    if (trozo.length() > 1500) { web.sendContent(trozo); trozo = ""; }
  }
  trozo += "]}";
  web.sendContent(trozo);
  web.sendContent("");
  free(cuenta); free(dron); free(alerta); free(maxp); free(etiq);
}

// CSV con los registros (para reentrenar en la app o descargar)
void apiDatos() {
  bool soloEtiquetados = web.arg("etiquetados") == "1";
  uint32_t desde = web.hasArg("desde") ? web.arg("desde").toInt() : 0;
  uint32_t hasta = web.hasArg("hasta") ? web.arg("hasta").toInt() : 0xFFFFFFFF;
  web.sendHeader("Content-Disposition", "attachment; filename=centinela_datos.csv");
  web.setContentLength(CONTENT_LENGTH_UNKNOWN);
  web.send(200, "text/csv", "");
  String trozo = "epoch,ventana,clase,p_dron,alerta,etiqueta";
  for (int i = 0; i < N_RASGOS; i++) { trozo += ","; trozo += NOMBRES_RASGOS[i]; }
  trozo += "\n";
  almacenRecorrer([&](const Registro& r) {
    if (soloEtiquetados && !r.etiqueta) return true;
    if (r.epoch < desde || r.epoch > hasta) return true;
    char linea[400];
    int m = snprintf(linea, sizeof(linea), "%lu,%lu,%u,%.3f,%u,%u", (unsigned long)r.epoch, (unsigned long)r.ventana,
                     r.clase, r.pDron / 255.0f, r.alerta, r.etiqueta);
    for (int i = 0; i < N_RASGOS && m < (int)sizeof(linea) - 12; i++) {
      if (i < N_RASGOS_WIFI) m += snprintf(linea + m, sizeof(linea) - m, ",%d", r.r[i]);
      else m += snprintf(linea + m, sizeof(linea) - m, ",%.3f", desescalar(i, r.r[i]));
    }
    linea[m++] = '\n';
    linea[m] = 0;
    trozo += linea;
    if (trozo.length() > 3000) { web.sendContent(trozo); trozo = ""; }
    return true;
  });
  web.sendContent(trozo);
  web.sendContent("");
}

void apiModelo() {
  String linea = web.arg("plain");
  linea.trim();
  if (!linea.startsWith("M,") || linea.length() > 4000) { responderJson(400, "{\"ok\":false,\"detalle\":\"linea de modelo invalida\"}"); return; }
  String detalle;
  bool ok = ordenS3(linea, detalle);
  if (ok) agregarEvento("MODELO nuevo cargado");
  responderJson(ok ? 200 : 502, String("{\"ok\":") + (ok ? "true" : "false") + ",\"detalle\":\"" + detalle + "\"}");
}

void apiModeloFabrica() {
  String detalle;
  bool ok = ordenS3("X", detalle);
  if (ok) agregarEvento("MODELO de fabrica");
  responderJson(ok ? 200 : 502, String("{\"ok\":") + (ok ? "true" : "false") + ",\"detalle\":\"" + detalle + "\"}");
}

void apiAjuste() {
  String clave = web.arg("clave"), valor = web.arg("valor");
  for (size_t i = 0; i < clave.length(); i++) if (!isalpha((unsigned char)clave[i]) && clave[i] != '_') clave = "";
  if (!clave.length()) { responderJson(400, "{\"ok\":false,\"detalle\":\"clave invalida\"}"); return; }
  String detalle;
  bool ok = ordenS3("C," + clave + "," + String(valor.toInt()), detalle);
  responderJson(ok ? 200 : 502, String("{\"ok\":") + (ok ? "true" : "false") + ",\"detalle\":\"" + detalle + "\"}");
}

void apiBorrarDatos() {
  almacenBorrar();
  agregarEvento("DATOS borrados");
  responderJson(200, "{\"ok\":true}");
}

// ------------------------------------------------------------------ WiFi
// Redes conocidas: las de secrets.h y hasta 5 que se agregan desde la app (quedan guardadas en
// la memoria del devkit). Se une a la conocida con mas senal. Si no ve ninguna, crea su red
// propia y vuelve a buscar cada 30 s mientras nadie este conectado a ella.
void anunciarMdns() {
  MDNS.end();
  if (MDNS.begin(NOMBRE_MDNS)) MDNS.addService("http", "tcp", 80);
}

void alConectar() {
  modoAP = false;
  caidoDesde = 0;
  if (apAbierta) apDesde = millis();
  Serial.printf(">>> WiFi \"%s\" OK. Abre en el celular: http://%s.local  (o http://%s)\n",
                WiFi.SSID().c_str(), NOMBRE_MDNS, WiFi.localIP().toString().c_str());
  agregarEvento(("WiFi " + WiFi.SSID() + " " + WiFi.localIP().toString()).c_str());
  configTime(0, 0, "pool.ntp.org", "time.google.com");
  anunciarMdns();
}

void abrirRedPropia() {
  modoAP = true;
  apAbierta = true;
  WiFi.disconnect(true);
  WiFi.mode(WIFI_AP);
  WiFi.softAP(AP_NOMBRE, AP_CLAVE);
  Serial.printf(">>> No encontre ninguna red conocida. Conecta el celular a \"%s\" (clave %s) y abre http://%s\n",
                AP_NOMBRE, AP_CLAVE, WiFi.softAPIP().toString().c_str());
  anunciarMdns();
}

void agregarConocida(const String& nombre, const String& clave, bool fija) {
  if (!nombre.length()) return;
  for (auto& r : redesConocidas)
    if (r.nombre == nombre) { if (!r.fija) r.clave = clave; return; }
  redesConocidas.push_back({nombre, clave, fija});
}

int redesDeLaApp() {
  int n = 0;
  for (auto& r : redesConocidas) if (!r.fija) n++;
  return n;
}

void cargarRedes() {
  redesConocidas.clear();
  agregarConocida(WIFI_NOMBRE, WIFI_CLAVE, true);
#ifdef WIFI2_NOMBRE
  agregarConocida(WIFI2_NOMBRE, WIFI2_CLAVE, true);
#endif
#ifdef WIFI3_NOMBRE
  agregarConocida(WIFI3_NOMBRE, WIFI3_CLAVE, true);
#endif
  Preferences p;
  p.begin("redes", true);
  int n = p.getUChar("n", 0);
  for (int i = 0; i < n && i < MAX_REDES_APP; i++)
    agregarConocida(p.getString(("s" + String(i)).c_str(), ""), p.getString(("c" + String(i)).c_str(), ""), false);
  p.end();
}

void guardarRedes() {
  Preferences p;
  p.begin("redes", false);
  p.clear();
  int i = 0;
  for (auto& r : redesConocidas) {
    if (r.fija || i >= MAX_REDES_APP) continue;
    p.putString(("s" + String(i)).c_str(), r.nombre);
    p.putString(("c" + String(i)).c_str(), r.clave);
    i++;
  }
  p.putUChar("n", i);
  p.end();
}

// Busca las redes cercanas y se une a la conocida con mas senal. Con soloEsta, prueba solo
// esa (aunque no salga en la busqueda, por si esta oculta).
bool conectarConocida(const String& soloEsta, uint32_t espera) {
  int n = WiFi.scanNetworks();
  const RedWiFi* elegida = nullptr;
  int mejor = -1000;
  for (int i = 0; i < n; i++)
    for (auto& r : redesConocidas)
      if (WiFi.SSID(i) == r.nombre && (!soloEsta.length() || r.nombre == soloEsta) && WiFi.RSSI(i) > mejor) {
        elegida = &r;
        mejor = WiFi.RSSI(i);
      }
  WiFi.scanDelete();
  if (!elegida && soloEsta.length())
    for (auto& r : redesConocidas) if (r.nombre == soloEsta) elegida = &r;
  if (!elegida) return false;
  Serial.printf("Probando \"%s\"", elegida->nombre.c_str());
  WiFi.begin(elegida->nombre.c_str(), elegida->clave.c_str());
  unsigned long inicio = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - inicio < espera) { delay(250); leerCable(); Serial.print("."); }
  Serial.println();
  if (WiFi.status() == WL_CONNECTED) return true;
  WiFi.disconnect();
  return false;
}

void iniciarWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.setHostname(NOMBRE_MDNS);
  cargarRedes();
  Serial.printf("Buscando redes conocidas (%d)...\n", (int)redesConocidas.size());
  if (conectarConocida("", 20000)) alConectar();
  else abrirRedPropia();
}

void vigilarWiFi() {
  static unsigned long ultimo = 0;
  if (pruebaPendiente) {                          // la app acaba de agregar una red
    pruebaPendiente = false;
    if (modoAP) WiFi.mode(WIFI_AP_STA);           // la red propia sigue para no perder la app
    if (conectarConocida(pruebaNombre, 15000)) { resultadoPrueba = "ok"; alConectar(); }
    else { resultadoPrueba = "fallo"; if (modoAP) WiFi.mode(WIFI_AP); }
    return;
  }
  if (modoAP) {
    if (millis() - ultimo < 30000 || WiFi.softAPgetStationNum() > 0) return;
    ultimo = millis();
    WiFi.mode(WIFI_AP_STA);
    if (conectarConocida("", 10000)) alConectar();
    else WiFi.mode(WIFI_AP);
    return;
  }
  // Ya hay WiFi: la red propia se apaga cuando el celular se va de ella, o a los 3 minutos.
  if (apAbierta && millis() - apDesde > 20000 && (WiFi.softAPgetStationNum() == 0 || millis() - apDesde > 180000)) {
    WiFi.softAPdisconnect(true);
    WiFi.mode(WIFI_STA);
    apAbierta = false;
    Serial.println("Red propia apagada: ya hay WiFi.");
  }
  if (WiFi.status() == WL_CONNECTED) { caidoDesde = 0; return; }
  if (!caidoDesde) caidoDesde = millis();
  if (millis() - ultimo < 15000) return;
  ultimo = millis();
  Serial.println("Se perdio el WiFi, buscando redes conocidas...");
  if (conectarConocida("", 10000)) alConectar();
  else if (millis() - caidoDesde > 60000) abrirRedPropia();   // un minuto sin red: la app sigue por CENTINELA-RF
}

void apiRedes() {
  String j = "{\"max\":"; j += MAX_REDES_APP; j += ",\"conocidas\":[";
  bool primera = true;
  for (auto& r : redesConocidas) {
    if (!primera) j += ",";
    primera = false;
    j += "{\"nombre\":\""; j += sinComillas(r.nombre); j += "\",\"fija\":"; j += r.fija ? "true" : "false"; j += "}";
  }
  j += "]}";
  responderJson(200, j);
}

void apiRedesCerca() {
  if (modoAP) WiFi.mode(WIFI_AP_STA);
  int n = WiFi.scanNetworks();
  String j = "{\"redes\":[";
  int puestas = 0;
  for (int i = 0; i < n; i++) {
    String nombre = sinComillas(WiFi.SSID(i));
    if (!nombre.length()) continue;
    bool repetida = false;                       // un mismo WiFi puede salir varias veces
    for (int k = 0; k < i; k++) if (WiFi.SSID(k) == WiFi.SSID(i)) repetida = true;
    if (repetida) continue;
    if (puestas++) j += ",";
    j += "{\"nombre\":\""; j += nombre;
    j += "\",\"rssi\":"; j += WiFi.RSSI(i);
    j += ",\"abierta\":"; j += WiFi.encryptionType(i) == WIFI_AUTH_OPEN ? "true" : "false"; j += "}";
  }
  WiFi.scanDelete();
  if (modoAP) WiFi.mode(WIFI_AP);
  j += "]}";
  responderJson(200, j);
}

void apiRedesAgregar() {
  String nombre = web.arg("nombre"), clave = web.arg("clave");
  nombre.trim();
  if (!nombre.length() || nombre.length() > 32) { responderJson(400, "{\"detalle\":\"el nombre debe tener de 1 a 32 caracteres\"}"); return; }
  if (clave.length() && (clave.length() < 8 || clave.length() > 63)) { responderJson(400, "{\"detalle\":\"la clave debe tener de 8 a 63 caracteres\"}"); return; }
  bool nueva = true;
  for (auto& r : redesConocidas) if (r.nombre == nombre) nueva = false;
  if (nueva && redesDeLaApp() >= MAX_REDES_APP) { responderJson(400, "{\"detalle\":\"ya hay 5 redes guardadas: quita una\"}"); return; }
  agregarConocida(nombre, clave, false);
  guardarRedes();
  agregarEvento(("WiFi guardada: " + nombre).c_str());
  pruebaNombre = nombre;
  bool probar = modoAP;                           // si ya hay WiFi no se cambia, para no cortar la app
  if (probar) { resultadoPrueba = "probando"; pruebaPendiente = true; }
  else resultadoPrueba = "";
  responderJson(200, String("{\"ok\":true,\"probando\":") + (probar ? "true" : "false") + "}");
}

void apiRedesBorrar() {
  String nombre = web.arg("nombre");
  for (size_t i = 0; i < redesConocidas.size(); i++)
    if (redesConocidas[i].nombre == nombre && !redesConocidas[i].fija) {
      redesConocidas.erase(redesConocidas.begin() + i);
      guardarRedes();
      responderJson(200, "{\"ok\":true}");
      return;
    }
  responderJson(404, "{\"detalle\":\"esa red no esta guardada desde la app\"}");
}

// ------------------------------------------------------------------ programa
void setup() {
  Serial.begin(115200);
  Serial2.setRxBufferSize(8192);
  Serial2.begin(115200, SERIAL_8N1, PIN_RX_S3, PIN_TX_S3);
  delay(300);
  Serial.println();
  Serial.println("=== devkit_centinela: S3 (cable) -> caja negra + app del celular + AWS ===");
  if (almacenIniciar())
    Serial.printf("Memoria: %lu registros guardados (caben ~%.1f h)\n", (unsigned long)almacenRegistros(),
                  2.0f * maxBytesArchivo / sizeof(Registro) * 2 / 3600);
  else
    Serial.println("ERROR: no se pudo abrir la memoria (LittleFS)");
  iniciarWiFi();

  web.on("/", HTTP_GET, apiApp);
  web.on("/api/estado", HTTP_GET, apiEstado);
  web.on("/api/hora", HTTP_POST, apiHora);
  web.on("/api/grabar", HTTP_POST, apiGrabar);
  web.on("/api/etiquetar", HTTP_POST, apiEtiquetar);
  web.on("/api/historial", HTTP_GET, apiHistorial);
  web.on("/api/datos", HTTP_GET, apiDatos);
  web.on("/api/modelo", HTTP_POST, apiModelo);
  web.on("/api/modelo_fabrica", HTTP_POST, apiModeloFabrica);
  web.on("/api/ajuste", HTTP_POST, apiAjuste);
  web.on("/api/borrar_datos", HTTP_POST, apiBorrarDatos);
  web.on("/api/redes", HTTP_GET, apiRedes);
  web.on("/api/redes/cerca", HTTP_GET, apiRedesCerca);
  web.on("/api/redes/agregar", HTTP_POST, apiRedesAgregar);
  web.on("/api/redes/borrar", HTTP_POST, apiRedesBorrar);
  web.onNotFound([]() { web.send(404, "text/plain", "no existe"); });
  web.begin();

#if USAR_AWS
  awsIniciar();
#endif
  Serial2.print("Q\n");   // pide al S3 su ficha (modelo y ajustes)
}

void loop() {
  leerCable();
  web.handleClient();
  vigilarWiFi();
#if USAR_AWS
  awsAtender();
#endif
  static unsigned long ultimoAviso = 0, ultimaFicha = 0;
  if (millis() - ultimoAviso > 15000) {
    ultimoAviso = millis();
    if (!s3.ultimoDato || millis() - s3.ultimoDato > 10000)
      Serial.println("... no llega nada del S3 (revisa GPIO 17 del S3 -> GPIO 16 del devkit y GND)");
  }
  if (!s3.hayFicha && millis() - ultimaFicha > 10000) { ultimaFicha = millis(); Serial2.print("Q\n"); }
  delay(1);
}
