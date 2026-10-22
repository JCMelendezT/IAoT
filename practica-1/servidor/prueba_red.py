#!/usr/bin/env python3
"""
Prueba de red de la Practica 1.

Sirve para comprobar UNA sola cosa: que el ESP32 (o el celular) puede
llegar hasta este computador por el WiFi de la casa.

- No necesita instalar nada (solo Python, que ya viene en Ubuntu).
- Cada mensaje que llegue se muestra en pantalla con la hora.
- Se detiene con Ctrl+C.

Uso (en Ubuntu):
    python3 /mnt/c/Users/juanc/Desktop/IAoT/practica-1/servidor/prueba_red.py
"""
import json
import subprocess
from datetime import datetime
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

PUERTO = 8000
RED_CASA = "192.168.40."


def ips_locales():
    """Direcciones IPv4 de este computador."""
    try:
        salida = subprocess.run(["hostname", "-I"], capture_output=True, text=True).stdout.split()
    except Exception:
        salida = []
    return [ip for ip in salida if ip.count(".") == 3]


class Manejador(BaseHTTPRequestHandler):
    def _responder(self, codigo, datos):
        cuerpo = json.dumps(datos, ensure_ascii=False).encode("utf-8")
        self.send_response(codigo)
        self.send_header("Content-Type", "application/json; charset=utf-8")
        self.send_header("Content-Length", str(len(cuerpo)))
        self.end_headers()
        self.wfile.write(cuerpo)

    def do_GET(self):
        # Para probar desde el navegador del celular.
        hora = datetime.now().strftime("%H:%M:%S")
        print(f"[{hora}] Visita desde {self.client_address[0]} (navegador)", flush=True)
        self._responder(200, {"ok": True, "mensaje": "El servidor de prueba esta vivo"})

    def do_POST(self):
        largo = int(self.headers.get("Content-Length", 0))
        texto = self.rfile.read(largo).decode("utf-8", "replace")
        hora = datetime.now().strftime("%H:%M:%S")
        try:
            datos = json.loads(texto)
            resumen = ", ".join(f"{k}={v}" for k, v in datos.items())
        except (json.JSONDecodeError, AttributeError):
            resumen = texto
        print(f"[{hora}] Llego de {self.client_address[0]} a {self.path}: {resumen}", flush=True)
        self._responder(200, {"ok": True})

    def log_message(self, *args):
        pass  # sin el registro tecnico por defecto


if __name__ == "__main__":
    print("=" * 60)
    print(f" Servidor de prueba escuchando en el puerto {PUERTO}")
    ips = ips_locales()
    de_la_casa = [ip for ip in ips if ip.startswith(RED_CASA)]
    if de_la_casa:
        print(f" IP de este PC en tu WiFi: {de_la_casa[0]}   <-- esta es la que necesitas")
        print(f" Prueba en el celular:     http://{de_la_casa[0]}:{PUERTO}")
    else:
        print(f" OJO: no encontre ninguna IP que empiece por {RED_CASA}")
        print(f" IPs encontradas: {', '.join(ips) or 'ninguna'}")
        print(" Revisa que el modo 'mirrored' de WSL este activo (ver instrucciones).")
    print(" Esperando mensajes... (Ctrl+C para salir)")
    print("=" * 60, flush=True)
    try:
        ThreadingHTTPServer(("0.0.0.0", PUERTO), Manejador).serve_forever()
    except KeyboardInterrupt:
        print("\nServidor detenido.")
