// Corre la red de gestos en el PC con el mismo codigo de la placa.
//   g++ -O2 -I.. probar_cnn.cpp -o probar_cnn
//   ./probar_cnn modelo_gestos.bin foto1.pgm foto2.pgm ...
// Imprime una linea por foto: nombre,p_nada,p_abierta,p_puno
#include <stdio.h>
#include <string>
#include <vector>
#include "cnn_gestos.h"

static bool leer(const char* ruta, std::vector<uint8_t>& datos) {
  FILE* f = fopen(ruta, "rb");
  if (!f) return false;
  uint8_t buf[4096];
  size_t n;
  while ((n = fread(buf, 1, sizeof buf, f)) > 0) datos.insert(datos.end(), buf, buf + n);
  fclose(f);
  return true;
}

int main(int argc, char** argv) {
  if (argc < 3) { fprintf(stderr, "uso: probar_cnn modelo.bin foto.pgm...\n"); return 2; }
  std::vector<uint8_t> m;
  if (!leer(argv[1], m)) { fprintf(stderr, "no pude leer %s\n", argv[1]); return 2; }
  gestos::Red red;
  const char* error = gestos::cargar(red, m.data(), m.size());
  if (error) { printf("ERROR %s\n", error); return 1; }
  for (int i = 2; i < argc; i++) {
    std::vector<uint8_t> d;
    if (!leer(argv[i], d)) { fprintf(stderr, "no pude leer %s\n", argv[i]); return 2; }
    int w, h, mx, pos = 0;
    if (sscanf((const char*)d.data(), "P5 %d %d %d%n", &w, &h, &mx, &pos) < 3 || w != gestos::ANCHO || h != gestos::ALTO) {
      fprintf(stderr, "%s: PGM raro\n", argv[i]);
      return 2;
    }
    pos++;   // un espacio despues del maximo
    float prob[gestos::CLASES];
    gestos::inferir(red, d.data() + pos, prob);
    std::string nombre(argv[i]);
    printf("%s,%.8f,%.8f,%.8f\n", nombre.substr(nombre.find_last_of('/') + 1).c_str(), prob[0], prob[1], prob[2]);
  }
  gestos::liberar(red);
  return 0;
}
