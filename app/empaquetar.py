"""
Empaqueta la app del celular para el devkit: mete entrenamiento.js dentro de index.html,
la comprime (gzip) y la escribe como arreglo de bytes en
firmware/devkit_centinela/app_web.h.

Uso:  python empaquetar.py      (correr cada vez que cambies index.html o entrenamiento.js)
"""
import gzip
import os

AQUI = os.path.dirname(os.path.abspath(__file__))
SALIDA = os.path.join(AQUI, "..", "firmware", "devkit_centinela", "app_web.h")
COPIA = os.path.join(AQUI, "app_completa.html")   # la misma app sin comprimir, para revisarla


def main():
    html = open(os.path.join(AQUI, "index.html"), encoding="utf-8").read()
    js = open(os.path.join(AQUI, "entrenamiento.js"), encoding="utf-8").read()
    assert "/*ENTRENAMIENTO*/" in html
    completo = html.replace("/*ENTRENAMIENTO*/", js)
    open(COPIA, "w", encoding="utf-8").write(completo)
    datos = gzip.compress(completo.encode("utf-8"), compresslevel=9, mtime=0)
    lineas = [", ".join("0x%02x" % b for b in datos[i:i + 20]) for i in range(0, len(datos), 20)]
    with open(SALIDA, "w", encoding="utf-8") as f:
        f.write("// App del celular comprimida con gzip. Generado por app/empaquetar.py.\n")
        f.write("// No editar a mano: cambia index.html o entrenamiento.js y vuelve a correr el script.\n")
        f.write("#pragma once\n#include <pgmspace.h>\n\n")
        f.write("const size_t APP_HTML_GZ_LEN = %d;\n" % len(datos))
        f.write("const uint8_t APP_HTML_GZ[] PROGMEM = {\n  " + ",\n  ".join(lineas) + "\n};\n")
    print("App: %d bytes -> %d comprimida -> %s" % (len(completo), len(datos), os.path.relpath(SALIDA, AQUI)))


if __name__ == "__main__":
    main()
