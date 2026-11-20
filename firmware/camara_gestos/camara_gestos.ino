// Camara de gestos para el detector de drones (ESP32-CAM AI-Thinker).
//
//   Mano abierta -> el devkit hace una prueba de sonido
//   Puno         -> el devkit se arma o se desarma (voz encendida o apagada)
//   Nada         -> no hace nada
//
// Reconoce los gestos con una red neuronal pequena (cnn_gestos.h) cuyos numeros se cargan desde
// la pagina de la camara (modelo_gestos.bin), y el programa se actualiza por WiFi (OTA).
// Solo la primera vez hace falta cable. Guia completa: README.md
//
// Placa en Arduino IDE: "AI Thinker ESP32-CAM"
// Esquema de particiones: "Minimal SPIFFS (1.9MB APP with OTA/190KB SPIFFS)"   <- OBLIGATORIO
#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <ArduinoOTA.h>
#include <Update.h>
#include <LittleFS.h>
#include <Preferences.h>
#include <HTTPClient.h>
#include "esp_camera.h"

#if __has_include("secrets.h")
#include "secrets.h"
#endif
#ifndef OTA_CLAVE
#define OTA_CLAVE "camara2026"
#endif
#ifndef DEVKIT_HOST
#define DEVKIT_HOST "detector.local"
#endif
#ifndef AP_NOMBRE
#define AP_NOMBRE "CAMARA-RF"
#endif
#ifndef AP_CLAVE
#define AP_CLAVE "camara2026"
#endif

#define GESTOS_ASIGNAR(n) (psramFound() ? ps_malloc(n) : malloc(n))
#include "cnn_gestos.h"
#include "pagina.h"

// ---- pines de la ESP32-CAM AI-Thinker ----
#define PIN_PWDN 32
#define PIN_RESET -1
#define PIN_XCLK 0
#define PIN_SIOD 26
#define PIN_SIOC 27
#define PIN_D7 35
#define PIN_D6 34
#define PIN_D5 39
#define PIN_D4 36
#define PIN_D3 21
#define PIN_D2 19
#define PIN_D1 18
#define PIN_D0 5
#define PIN_VSYNC 25
#define PIN_HREF 23
#define PIN_PCLK 22
#define PIN_LED_ROJO 33        // led chico de atras: se enciende con LOW

// ---- regla de gestos (igual a la del entrenador, detector/gestos/entrenar_gestos.py) ----
static const int VENTANA = 6, MINIMO = 5;
static const float CONFIANZA = 0.80f;
static const uint32_t PERIODO_MS = 250;           // una foto cada 250 ms, o mas lento si la red tarda
static const uint32_t ENFRIAR_MS = 2500;          // pausa tras ejecutar una accion

Preferences pref;
WebServer server(80);

// ---- estado ----
static bool modoSeguro = false, camaraOk = false, estable = false, enAP = false;
static int fallosArranque = 0;
static uint8_t ultima[gestos::ANCHO * gestos::ALTO];      // la foto 80x60 que ve la red
static uint8_t pgm[15 + gestos::ANCHO * gestos::ALTO];
static uint32_t seq = 0, tUltimo = 0, tAccion = 0, tWiFiRev = 0, tSinWiFi = 0, tUltimaFoto = 0;
static int fotosMalas = 0;
static gestos::Red red;
static String errorModelo = "";
static float prob[gestos::CLASES] = {1, 0, 0};
static int ventana[VENTANA];
static bool libre = true;
static int gestoFirme = 0;                // 0 nada, 1 abierta, 2 puno (lo que se ve ahora)
static uint32_t msInferencia = 0;
static float fps = 0;
static String ultimaAccion = "ninguna", resultadoAccion = "";
static uint32_t tUltimaAccion = 0;

// ---- WiFi ----
static String redesN[4], redesC[4];
static int nRedes = 0;
static String devkitHost = DEVKIT_HOST;
static IPAddress devkitIp;                // direccion ya resuelta (se borra si falla)

String escapar(const String& s) {
  String o;
  for (size_t i = 0; i < s.length(); i++) {
    char c = s[i];
    if (c == '"' || c == '\\') { o += '\\'; o += c; }
    else if ((uint8_t)c < 32) o += ' ';
    else o += c;
  }
  return o;
}

void led(bool on) { digitalWrite(PIN_LED_ROJO, on ? LOW : HIGH); }
void parpadear(int veces, int ms) {
  for (int i = 0; i < veces; i++) { led(true); delay(ms); led(false); delay(ms); }
}

void cargarRedes() {
  nRedes = 0;
#ifdef WIFI_NOMBRE
  redesN[nRedes] = WIFI_NOMBRE; redesC[nRedes] = WIFI_CLAVE; nRedes++;
#endif
  for (int i = 0; i < 3 && nRedes < 4; i++) {
    String n = pref.getString(("n" + String(i)).c_str(), "");
    if (n.length()) { redesN[nRedes] = n; redesC[nRedes] = pref.getString(("c" + String(i)).c_str(), ""); nRedes++; }
  }
}

void guardarRed(const String& nombre, const String& clave) {
  int hueco = -1;
  for (int i = 0; i < 3; i++) {
    String n = pref.getString(("n" + String(i)).c_str(), "");
    if (n == nombre) { hueco = i; break; }
    if (n.length() == 0 && hueco < 0) hueco = i;
  }
  if (hueco < 0) { hueco = pref.getUChar("rr", 0) % 3; pref.putUChar("rr", hueco + 1); }
  pref.putString(("n" + String(hueco)).c_str(), nombre);
  pref.putString(("c" + String(hueco)).c_str(), clave);
  cargarRedes();
}

void iniciarAP() {
  WiFi.mode(WIFI_AP);
  WiFi.softAP(AP_NOMBRE, AP_CLAVE);
  enAP = true;
  Serial.printf("Sin WiFi conocido. Red propia: %s (clave %s) -> http://192.168.4.1\n", AP_NOMBRE, AP_CLAVE);
}

// Se une al WiFi conocido con mas senal. Devuelve true si lo logra.
bool conectar() {
  WiFi.mode(WIFI_STA);
  WiFi.setHostname("camara");
  WiFi.setAutoReconnect(true);
  int n = WiFi.scanNetworks(false, false);
  int mejor = -1, mejorRssi = -1000;
  for (int i = 0; i < n; i++)
    for (int k = 0; k < nRedes; k++)
      if (WiFi.SSID(i) == redesN[k] && WiFi.RSSI(i) > mejorRssi) { mejor = k; mejorRssi = WiFi.RSSI(i); }
  WiFi.scanDelete();
  if (mejor < 0) return false;
  Serial.printf("Conectando a %s...\n", redesN[mejor].c_str());
  WiFi.begin(redesN[mejor].c_str(), redesC[mejor].c_str());
  uint32_t t0 = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t0 < 15000) delay(250);
  if (WiFi.status() != WL_CONNECTED) return false;
  enAP = false;
  Serial.print("WiFi listo, IP "); Serial.println(WiFi.localIP());
  return true;
}

void iniciarRed() {
  if (!conectar()) iniciarAP();
  if (MDNS.begin("camara")) MDNS.addService("http", "tcp", 80);
}

void vigilarWiFi() {
  if (millis() - tWiFiRev < 5000) return;
  tWiFiRev = millis();
  if (enAP) {
    // cada 90 s vuelve a buscar un WiFi conocido, si nadie esta conectado a la red propia
    static uint32_t tIntento = 0;
    if (millis() - tIntento > 90000 && WiFi.softAPgetStationNum() == 0) {
      tIntento = millis();
      if (conectar()) { MDNS.end(); MDNS.begin("camara"); MDNS.addService("http", "tcp", 80); }
      else iniciarAP();
    }
    return;
  }
  if (WiFi.status() == WL_CONNECTED) { tSinWiFi = 0; return; }
  if (!tSinWiFi) tSinWiFi = millis();
  if (millis() - tSinWiFi > 30000) {                 // 30 s sin WiFi: busca otra red o abre la propia
    tSinWiFi = 0;
    if (!conectar()) iniciarAP();
  }
}

// ---- devkit centinela ----
bool resolverDevkit(IPAddress& ip) {
  if (devkitIp != IPAddress(0, 0, 0, 0)) { ip = devkitIp; return true; }
  if (ip.fromString(devkitHost)) { devkitIp = ip; return true; }
  String n = devkitHost;
  if (n.endsWith(".local")) n.remove(n.length() - 6);
  IPAddress r = MDNS.queryHost(n, 2000);
  if (r == IPAddress(0, 0, 0, 0)) WiFi.hostByName(devkitHost.c_str(), r);
  if (r == IPAddress(0, 0, 0, 0)) return false;
  devkitIp = r; ip = r;
  return true;
}

// Devuelve el codigo HTTP (o -1 si no se pudo llegar al devkit). La respuesta queda en cuerpo.
int devkit(bool post, const String& ruta, String& cuerpo) {
  for (int intento = 0; intento < 2; intento++) {
    IPAddress ip;
    if (!resolverDevkit(ip)) return -1;
    HTTPClient h;
    h.setTimeout(2500);
    h.setConnectTimeout(2000);
    if (!h.begin("http://" + ip.toString() + ruta)) return -1;
    int code = post ? h.POST("") : h.GET();
    if (code > 0) { cuerpo = h.getString(); h.end(); return code; }
    h.end();
    devkitIp = IPAddress(0, 0, 0, 0);               // quiza cambio de IP: vuelve a buscarlo
  }
  return -1;
}

void anotar(const String& accion, const String& resultado) {
  ultimaAccion = accion; resultadoAccion = resultado; tUltimaAccion = millis();
  Serial.printf("Accion: %s -> %s\n", accion.c_str(), resultado.c_str());
}

void accionProbarSonido() {
  String r;
  int c = devkit(true, "/api/ajuste?clave=probar_voz&valor=1", r);
  if (c == 200) { anotar("Mano abierta: probar sonido", "ok"); parpadear(1, 150); }
  else { anotar("Mano abierta: probar sonido", c < 0 ? "no encuentro el devkit" : "el devkit respondio " + String(c)); parpadear(3, 80); }
}

void accionArmarDesarmar() {
  String r;
  int c = devkit(false, "/api/estado", r);
  if (c != 200) { anotar("Puno: armar o desarmar", c < 0 ? "no encuentro el devkit" : "el devkit respondio " + String(c)); parpadear(3, 80); return; }
  int i = r.indexOf("\"voz\":");
  if (i < 0) { anotar("Puno: armar o desarmar", "no encuentro el ajuste voz"); parpadear(3, 80); return; }
  i += 6;
  while (i < (int)r.length() && r[i] == ' ') i++;
  int actual = (i < (int)r.length() && r[i] == '1') ? 1 : 0;
  int nuevo = 1 - actual;
  c = devkit(true, "/api/ajuste?clave=voz&valor=" + String(nuevo), r);
  if (c == 200) { anotar("Puno: armar o desarmar", nuevo ? "ARMADO (voz encendida)" : "DESARMADO (voz apagada)"); parpadear(nuevo ? 2 : 1, 200); }
  else { anotar("Puno: armar o desarmar", "no pude cambiar la voz (" + String(c) + ")"); parpadear(3, 80); }
}

// ---- camara y reconocimiento ----
bool iniciarCamara() {
  camera_config_t c = {};
  c.ledc_channel = LEDC_CHANNEL_0; c.ledc_timer = LEDC_TIMER_0;
  c.pin_d0 = PIN_D0; c.pin_d1 = PIN_D1; c.pin_d2 = PIN_D2; c.pin_d3 = PIN_D3;
  c.pin_d4 = PIN_D4; c.pin_d5 = PIN_D5; c.pin_d6 = PIN_D6; c.pin_d7 = PIN_D7;
  c.pin_xclk = PIN_XCLK; c.pin_pclk = PIN_PCLK; c.pin_vsync = PIN_VSYNC; c.pin_href = PIN_HREF;
  c.pin_sccb_sda = PIN_SIOD; c.pin_sccb_scl = PIN_SIOC;
  c.pin_pwdn = PIN_PWDN; c.pin_reset = PIN_RESET;
  c.xclk_freq_hz = 20000000;
  c.pixel_format = PIXFORMAT_GRAYSCALE;
  c.frame_size = FRAMESIZE_QQVGA;                    // 160x120
  c.jpeg_quality = 12;
  c.fb_count = psramFound() ? 2 : 1;
  c.fb_location = psramFound() ? CAMERA_FB_IN_PSRAM : CAMERA_FB_IN_DRAM;
  c.grab_mode = CAMERA_GRAB_LATEST;
  esp_err_t e = esp_camera_init(&c);
  if (e != ESP_OK) { Serial.printf("La camara no inicia (0x%x). Revisa el cable de la camara y la alimentacion de 5V.\n", e); return false; }
  return true;
}

// 160x120 -> 80x60 promediando cada cuadro de 2x2 (igual que el entrenador)
bool tomarFoto() {
  camera_fb_t* fb = esp_camera_fb_get();
  if (!fb) return false;
  bool ok = fb->format == PIXFORMAT_GRAYSCALE && fb->width == 160 && fb->height == 120;
  if (ok) {
    for (int y = 0; y < gestos::ALTO; y++) {
      const uint8_t* a = fb->buf + (size_t)(2 * y) * 160;
      const uint8_t* b = a + 160;
      uint8_t* o = ultima + y * gestos::ANCHO;
      for (int x = 0; x < gestos::ANCHO; x++)
        o[x] = (uint8_t)((a[2 * x] + a[2 * x + 1] + b[2 * x] + b[2 * x + 1] + 2) >> 2);
    }
  }
  esp_camera_fb_return(fb);
  return ok;
}

void reiniciarVentana() { for (int i = 0; i < VENTANA; i++) ventana[i] = 0; }

void decidir() {
  int c = 0;
  for (int k = 1; k < gestos::CLASES; k++) if (prob[k] > prob[c]) c = k;
  static int pos = 0;
  ventana[pos++ % VENTANA] = (c != 0 && prob[c] >= CONFIANZA) ? c : 0;
  int votos[3] = {0, 0, 0};
  for (int i = 0; i < VENTANA; i++) votos[ventana[i]]++;
  int k = votos[2] > votos[1] ? 2 : 1;
  gestoFirme = votos[k] >= MINIMO ? k : 0;
  if (libre && gestoFirme && millis() - tAccion > ENFRIAR_MS) {
    libre = false;
    if (gestoFirme == 1) accionProbarSonido(); else accionArmarDesarmar();
    tAccion = millis();
    reiniciarVentana();
  } else if (!libre && votos[0] >= VENTANA - 1) {
    libre = true;                                       // volvio a "nada": listo para otro gesto
  }
}

void ciclo() {
  uint32_t t0 = millis();
  if (!tomarFoto()) {
    if (++fotosMalas > 20) { Serial.println("La camara no entrega fotos. Reinicio."); delay(200); ESP.restart(); }
    return;
  }
  fotosMalas = 0;
  seq++;
  if (red.listo) {
    uint32_t a = micros();
    gestos::inferir(red, ultima, prob);
    msInferencia = (micros() - a) / 1000;
    decidir();
  }
  uint32_t ahora = millis();
  if (tUltimaFoto) { float f = 1000.0f / (float)(ahora - tUltimaFoto + 1); fps = fps ? 0.8f * fps + 0.2f * f : f; }
  tUltimaFoto = ahora;
  tUltimo = t0;
}

// ---- modelo ----
void cargarModeloGuardado() {
  errorModelo = "";
  File f = LittleFS.open("/modelo.bin", "r");
  if (!f) { errorModelo = "todavia no has subido un modelo"; return; }
  size_t n = f.size();
  uint8_t* b = n && n < 200000 ? (uint8_t*)GESTOS_ASIGNAR(n) : nullptr;
  if (!b) { f.close(); errorModelo = "modelo de tamano raro"; LittleFS.remove("/modelo.bin"); return; }
  f.read(b, n); f.close();
  gestos::Red nueva;
  const char* e = gestos::cargar(nueva, b, n);
  free(b);
  if (e) { errorModelo = e; LittleFS.remove("/modelo.bin"); return; }
  gestos::liberar(red); red = nueva;
}

static File archivoModelo;
static String respModelo; static int codModelo = 200;

void modeloSubiendo() {
  HTTPUpload& u = server.upload();
  if (u.status == UPLOAD_FILE_START) { archivoModelo = LittleFS.open("/modelo.tmp", "w"); }
  else if (u.status == UPLOAD_FILE_WRITE) { if (archivoModelo) archivoModelo.write(u.buf, u.currentSize); }
  else if (u.status == UPLOAD_FILE_END) { if (archivoModelo) archivoModelo.close(); }
  else if (u.status == UPLOAD_FILE_ABORTED) { if (archivoModelo) archivoModelo.close(); LittleFS.remove("/modelo.tmp"); }
}

void modeloListo() {
  File f = LittleFS.open("/modelo.tmp", "r");
  size_t n = f ? f.size() : 0;
  uint8_t* b = n && n < 200000 ? (uint8_t*)GESTOS_ASIGNAR(n) : nullptr;
  if (!b) { if (f) f.close(); LittleFS.remove("/modelo.tmp"); server.send(400, "text/plain", "Archivo vacio o demasiado grande"); return; }
  f.read(b, n); f.close();
  gestos::Red nueva;
  const char* e = gestos::cargar(nueva, b, n);
  free(b);
  if (e) { LittleFS.remove("/modelo.tmp"); server.send(400, "text/plain", String("Modelo rechazado: ") + e); return; }
  gestos::liberar(red); red = nueva;
  LittleFS.remove("/modelo.bin");
  LittleFS.rename("/modelo.tmp", "/modelo.bin");
  errorModelo = ""; reiniciarVentana(); libre = true;
  server.send(200, "text/plain", "Modelo cargado");
}

// ---- cambio de programa por WiFi ----
static bool otaOk = false; static String otaError = "";

void otaSubiendo() {
  HTTPUpload& u = server.upload();
  if (u.status == UPLOAD_FILE_START) {
    otaOk = server.arg("clave") == OTA_CLAVE; otaError = "";
    if (!otaOk) { otaError = "clave incorrecta"; return; }
    if (!Update.begin(UPDATE_SIZE_UNKNOWN)) { otaOk = false; otaError = Update.errorString(); }
  } else if (u.status == UPLOAD_FILE_WRITE) {
    if (otaOk && Update.write(u.buf, u.currentSize) != u.currentSize) { otaOk = false; otaError = Update.errorString(); }
  } else if (u.status == UPLOAD_FILE_END) {
    if (otaOk && !Update.end(true)) { otaOk = false; otaError = Update.errorString(); }
  } else if (u.status == UPLOAD_FILE_ABORTED) {
    Update.abort(); otaOk = false; otaError = "se corto la subida";
  }
}

void otaListo() {
  if (otaOk && !Update.hasError()) {
    pref.putUChar("fallos", 0);
    server.send(200, "text/plain", "Listo. La camara se reinicia con el programa nuevo.");
    delay(600);
    ESP.restart();
  } else {
    server.send(400, "text/plain", "No se actualizo: " + (otaError.length() ? otaError : String("error")));
  }
}

// ---- paginas ----
void apiEstado() {
  String s = "{";
  s += "\"modo\":\"" + String(modoSeguro ? "seguro" : (enAP ? "ap" : "normal")) + "\",";
  s += "\"camara\":" + String(camaraOk ? "true" : "false") + ",";
  s += "\"ip\":\"" + (enAP ? WiFi.softAPIP().toString() : WiFi.localIP().toString()) + "\",";
  s += "\"wifi\":\"" + escapar(enAP ? String(AP_NOMBRE) : WiFi.SSID()) + "\",";
  s += "\"rssi\":" + String(enAP ? 0 : WiFi.RSSI()) + ",";
  s += "\"fallos\":" + String(fallosArranque) + ",";
  s += "\"seq\":" + String(seq) + ",";
  s += "\"fps\":" + String(fps, 1) + ",";
  s += "\"ms\":" + String(msInferencia) + ",";
  s += "\"modelo\":" + String(red.listo ? "true" : "false") + ",";
  s += "\"modeloSuma\":\"" + (red.listo ? String(red.suma, HEX) : String("")) + "\",";
  s += "\"modeloError\":\"" + escapar(errorModelo) + "\",";
  s += "\"prob\":[" + String(prob[0], 3) + "," + String(prob[1], 3) + "," + String(prob[2], 3) + "],";
  s += "\"gesto\":" + String(gestoFirme) + ",";
  s += "\"libre\":" + String(libre ? "true" : "false") + ",";
  s += "\"devkit\":\"" + escapar(devkitHost) + "\",";
  s += "\"devkitIp\":\"" + (devkitIp == IPAddress(0, 0, 0, 0) ? String("") : devkitIp.toString()) + "\",";
  s += "\"accion\":\"" + escapar(ultimaAccion) + "\",";
  s += "\"resultado\":\"" + escapar(resultadoAccion) + "\",";
  s += "\"haceSeg\":" + String(tUltimaAccion ? (millis() - tUltimaAccion) / 1000 : -1) + ",";
  s += "\"memoria\":" + String(ESP.getFreeHeap()) + ",";
  s += "\"psram\":" + String(ESP.getFreePsram()) + ",";
  s += "\"redes\":[";
  for (int i = 0; i < nRedes; i++) s += String(i ? "," : "") + "\"" + escapar(redesN[i]) + "\"";
  s += "]}";
  server.sendHeader("Cache-Control", "no-store");
  server.send(200, "application/json", s);
}

void apiEntrada() {
  if (!camaraOk || modoSeguro || !seq) { server.send(503, "text/plain", "sin foto"); return; }
  int n = snprintf((char*)pgm, 15, "P5\n%d %d\n255\n", gestos::ANCHO, gestos::ALTO);
  memcpy(pgm + n, ultima, sizeof(ultima));
  server.sendHeader("Cache-Control", "no-store");
  server.sendHeader("X-Seq", String(seq));
  server.send_P(200, "image/x-portable-graymap", (PGM_P)pgm, n + sizeof(ultima));
}

void apiWiFi() {
  String n = server.arg("nombre"), c = server.arg("clave");
  n.trim();
  if (!n.length() || n.length() > 32 || c.length() > 63) { server.send(400, "text/plain", "Nombre o clave no validos"); return; }
  guardarRed(n, c);
  server.send(200, "text/plain", "Guardada. Se usa la proxima vez que busque WiFi (reinicia la camara para probarla ya).");
}

void apiDevkit() {
  String h = server.arg("host");
  h.trim();
  if (!h.length() || h.length() > 60) { server.send(400, "text/plain", "Direccion no valida"); return; }
  devkitHost = h; devkitIp = IPAddress(0, 0, 0, 0);
  pref.putString("devkit", h);
  server.send(200, "text/plain", "Guardado");
}

void apiProbarDevkit() {
  String r;
  int c = devkit(false, "/api/estado", r);
  if (c == 200) server.send(200, "text/plain", "El devkit responde (" + devkitIp.toString() + ")");
  else server.send(502, "text/plain", c < 0 ? "No encuentro el devkit. Pon su IP en vez del nombre." : "El devkit respondio " + String(c));
}

void apiReiniciar() {
  pref.putUChar("fallos", 0);
  server.send(200, "text/plain", "Reiniciando...");
  delay(500);
  ESP.restart();
}

void configurarServidor() {
  server.on("/", HTTP_GET, []() { server.send_P(200, "text/html; charset=utf-8", PAGINA); });
  server.on("/api/estado", HTTP_GET, apiEstado);
  server.on("/api/entrada.pgm", HTTP_GET, apiEntrada);
  server.on("/api/wifi", HTTP_POST, apiWiFi);
  server.on("/api/devkit", HTTP_POST, apiDevkit);
  server.on("/api/probar_devkit", HTTP_POST, apiProbarDevkit);
  server.on("/api/modelo", HTTP_POST, modeloListo, modeloSubiendo);
  server.on("/actualizar", HTTP_POST, otaListo, otaSubiendo);
  server.on("/api/reiniciar", HTTP_POST, apiReiniciar);     // tambien saca del modo seguro
  server.onNotFound([]() { server.send(404, "text/plain", "no existe"); });
  server.begin();
}

void setup() {
  Serial.begin(115200);
  pinMode(PIN_LED_ROJO, OUTPUT); led(false);
  pref.begin("camara", false);
  fallosArranque = pref.getUChar("fallos", 0);
  if (fallosArranque < 100) pref.putUChar("fallos", fallosArranque + 1);   // si el arranque sale bien, se borra a los 30 s
  fallosArranque++;
  modoSeguro = fallosArranque >= 3;
  devkitHost = pref.getString("devkit", DEVKIT_HOST);
  reiniciarVentana();
  cargarRedes();
  Serial.printf("\nCamara de gestos. Arranque %d%s\n", fallosArranque, modoSeguro ? " -> MODO SEGURO (solo WiFi y actualizar)" : "");

  if (!modoSeguro) {
    LittleFS.begin(true);
    camaraOk = iniciarCamara();
    if (camaraOk) cargarModeloGuardado();
  }
  iniciarRed();
  ArduinoOTA.setHostname("camara");
  ArduinoOTA.setPassword(OTA_CLAVE);
  ArduinoOTA.onEnd([]() { pref.putUChar("fallos", 0); });
  ArduinoOTA.begin();
  configurarServidor();
  Serial.println("Pagina: http://camara.local  (o la IP de arriba)");
}

void loop() {
  server.handleClient();
  ArduinoOTA.handle();
  vigilarWiFi();
  if (!estable && !modoSeguro && millis() > 30000) { pref.putUChar("fallos", 0); estable = true; }
  if (!modoSeguro && camaraOk && millis() - tUltimo >= PERIODO_MS) ciclo();
}
