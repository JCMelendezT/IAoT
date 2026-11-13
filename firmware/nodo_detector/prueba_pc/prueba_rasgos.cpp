// Prueba en el PC del calculo de rasgos del firmware (rasgos_detector.h + modelo_detector.h).
// Ventana = filas del nRF24 con ms del ESP32 en (ms_W - 2000, ms_W], igual que ml/rasgos.py.
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include "rasgos_detector.h"
#include "modelo_detector.h"
struct W { double t; long v[14]; };
int main(int argc, char** argv) {
  int mapa[DETECTOR_N_RASGOS];
  const char* falta = rasgosPrepararMapa(DETECTOR_RASGOS, DETECTOR_N_RASGOS, mapa);
  if (falta) { printf("FALTA %s\n", falta); return 1; }
  for (int a = 1; a < argc; a++) {
    FILE* f = fopen(argv[a], "r"); char linea[600]; std::string sesion;
    std::vector<long> ms; std::vector<std::vector<uint8_t>> filas; std::vector<W> ws;
    while (fgets(linea, sizeof linea, f)) {
      if (linea[0] == '#') { if (!strncmp(linea, "# sesion: ", 10)) { sesion = linea + 10; sesion.erase(sesion.find_last_not_of("\r\n") + 1); } continue; }
      double t; char dato[560]; if (sscanf(linea, "%lf %559s", &t, dato) != 2) continue;
      if (!strncmp(dato, "N,", 2)) {
        char* p = strchr(dato + 2, ','); if (!p || strlen(p + 1) != RD_CANALES) continue;
        std::vector<uint8_t> v(RD_CANALES); for (int c = 0; c < RD_CANALES; c++) v[c] = p[1 + c] - '0';
        ms.push_back(atol(dato + 2)); filas.push_back(v);
      } else if (!strncmp(dato, "W,", 2)) {
        W w; w.t = t;
        if (sscanf(dato + 2, "%ld,%ld,%ld,%ld,%ld,%ld,%ld,%ld,%ld,%ld,%ld,%ld,%ld,%ld", &w.v[0], &w.v[1], &w.v[2], &w.v[3], &w.v[4], &w.v[5],
                   &w.v[6], &w.v[7], &w.v[8], &w.v[9], &w.v[10], &w.v[11], &w.v[12], &w.v[13]) == 14) ws.push_back(w);
      }
    }
    fclose(f);
    for (const W& w : ws) {
      VentanaWifi vw = {(int32_t)w.v[3], (int32_t)w.v[4], (int32_t)w.v[5], (int32_t)w.v[6], (int32_t)w.v[7],
                        (int32_t)w.v[8], (int32_t)w.v[9], (int32_t)w.v[10], (int32_t)w.v[11]};
      VentanaEspectro e; espectroLimpiar(e);
      for (size_t i = 0; i < ms.size(); i++) if (ms[i] > w.v[0] - 2000 && ms[i] <= w.v[0]) espectroAgregarFila(e, filas[i].data());
      float todos[RD_N_TODOS], x[DETECTOR_N_RASGOS], p[3];
      if (!rasgosCalcular(vw, e, todos)) continue;
      rasgosElegir(todos, mapa, DETECTOR_N_RASGOS, x);
      int c = detectorClasificar(x, p);
      printf("%s,%.3f", sesion.c_str(), w.t);
      for (int i = 0; i < DETECTOR_N_RASGOS; i++) printf(",%.6g", x[i]);
      printf(",%.6f,%.6f,%.6f,%s\n", p[0], p[1], p[2], DETECTOR_CLASES[c]);
    }
  }
}
