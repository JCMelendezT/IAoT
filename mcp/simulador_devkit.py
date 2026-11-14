"""Devkit falso para probar el servidor MCP sin la placa.

    python simulador_devkit.py          # escucha en http://127.0.0.1:8099
"""
import json
import time
from http.server import BaseHTTPRequestHandler, HTTPServer
from urllib.parse import parse_qs, urlparse

AHORA = int(time.time())


def historial(horas):
    n = horas * 60
    inicio = (AHORA // 60 - n + 1) * 60
    minutos = [[30, 0, 0, 20, 0] for _ in range(n)]
    for i in range(n - 200, n - 195):          # alerta falsa, ya etiquetada como fondo
        minutos[i] = [30, 4, 2, 190, 1]
    for i in range(n - 60, n - 52):            # alerta de dron de verdad
        minutos[i] = [30, 25, 20, 250, 3]
    for i in range(n - 400, n - 380):          # hueco: el S3 estuvo apagado
        minutos[i] = [0, 0, 0, 0, 0]
    return {"inicio": inicio, "min": minutos}


class H(BaseHTTPRequestHandler):
    def do_GET(self):
        u = urlparse(self.path)
        if u.path == "/api/estado":
            cuerpo = {"eventos": [
                {"epoch": AHORA - 120, "texto": "ALERTA dron p=0.98"},
                {"epoch": AHORA - 3600, "texto": "WiFi iPhone-Juan 172.20.10.2"},
            ]}
        elif u.path == "/api/historial":
            cuerpo = historial(int(parse_qs(u.query).get("horas", ["1"])[0]))
        else:
            self.send_error(404)
            return
        datos = json.dumps(cuerpo).encode()
        self.send_response(200)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(datos)))
        self.end_headers()
        self.wfile.write(datos)

    def log_message(self, *a):
        pass


if __name__ == "__main__":
    print("Devkit falso en http://127.0.0.1:8099")
    HTTPServer(("127.0.0.1", 8099), H).serve_forever()
