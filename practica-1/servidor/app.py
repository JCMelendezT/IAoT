#!/usr/bin/env python3
"""
Servidor de la Practica 1 — detector de drones.

Recibe lo que manda el nodo (por el devkit que hace de pasarela) y lo guarda
DOS veces: en archivos CSV (carpeta ../datos) y en PostgreSQL.

Direcciones:
    POST /api/latido    "estoy vivo" de la placa
    POST /api/ventanas  resumen de 2 segundos (WiFi + espectro nRF24)
    POST /api/eventos   alerta u otro evento decidido en la placa
    GET  /              pagina con las ultimas lecturas (se actualiza sola)
    GET  /api/resumen   lo mismo que la pagina, en JSON
    GET  /docs          pagina automatica para probar la API a mano

Configuracion en el archivo .env de esta carpeta (lo crea preparar_ubuntu.sh).
Para arrancarlo:  bash arrancar.sh
"""
from __future__ import annotations

import csv
from contextlib import asynccontextmanager
import subprocess
import threading
from datetime import datetime, timezone
from pathlib import Path
from typing import Optional

import psycopg
from psycopg.rows import dict_row
from fastapi import FastAPI, HTTPException, Request
from fastapi.responses import FileResponse
from pydantic import BaseModel, Field

CARPETA = Path(__file__).resolve().parent
CARPETA_DATOS = CARPETA.parent / "datos"

# Banda donde aparecio el DJI Neo 2 con control (2423..2456 MHz) = columnas 23..56.
BANDA_DRON = (23, 57)


# ---------------------------------------------------------------- configuracion
def leer_env(ruta: Path) -> dict:
    """Lee un archivo .env sencillo (CLAVE=valor por linea)."""
    valores = {}
    if ruta.exists():
        for linea in ruta.read_text(encoding="utf-8").splitlines():
            linea = linea.strip()
            if linea and not linea.startswith("#") and "=" in linea:
                k, v = linea.split("=", 1)
                valores[k.strip()] = v.strip().strip('"').strip("'")
    return valores


ENV = leer_env(CARPETA / ".env")
PUERTO = int(ENV.get("PUERTO", "8000"))
# Regla provisional: se calibra con las grabaciones de la Fase 4.
UMBRAL_BANDA = float(ENV.get("UMBRAL_BANDA", "2.5"))
CONEXION_BD = (
    f"host={ENV.get('DB_HOST', 'localhost')} port={ENV.get('DB_PUERTO', '5432')} "
    f"dbname={ENV.get('DB_NOMBRE', 'iaot')} user={ENV.get('DB_USUARIO', 'iaot')} "
    f"password={ENV.get('DB_CLAVE', '')} connect_timeout=3"
)


def bd():
    return psycopg.connect(CONEXION_BD, autocommit=True, row_factory=dict_row)


def crear_tablas():
    with bd() as con:
        con.execute((CARPETA / "esquema.sql").read_text(encoding="utf-8"))


# ---------------------------------------------------------------- lo que llega
class Latido(BaseModel):
    nodo: str = Field(min_length=1, max_length=32)
    contador: int = Field(ge=0)
    ms: int = Field(ge=0)
    rssi: Optional[int] = Field(default=None, ge=-128, le=0)  # vacio = placa sin WiFi propio (va por cable)


class Ventana(BaseModel):
    nodo: str = Field(min_length=1, max_length=32)
    ms: int = Field(ge=0)
    ventana: int = Field(ge=0)
    canal: int = Field(ge=0, le=14)
    tramas: int = Field(ge=0)
    aparatos: int = Field(ge=0)
    mgmt: int = Field(ge=0)
    datos: int = Field(ge=0)
    tam_prom: float = Field(ge=0)
    rssi_prom: float = Field(ge=-128, le=0)
    rssi_max: int = Field(ge=-128, le=0)
    tramas_debiles: int = Field(ge=0)
    aparatos_fuertes: int = Field(ge=0)
    tramas_dji: int = Field(ge=0)
    rssi_max_dji: int = Field(ge=-128, le=0)
    espectro: Optional[str] = Field(default=None, pattern=r"^[0-8]{126}$")


class Evento(BaseModel):
    nodo: str = Field(min_length=1, max_length=32)
    ms: int = Field(ge=0)
    tipo: str = Field(min_length=1, max_length=40)
    clase: Optional[str] = Field(default=None, max_length=40)
    probabilidad: Optional[float] = Field(default=None, ge=0, le=1)
    detalle: Optional[str] = Field(default=None, max_length=300)


# ---------------------------------------------------------------- reglas
def nivel_banda(espectro: Optional[str]) -> Optional[float]:
    if not espectro:
        return None
    trozo = espectro[BANDA_DRON[0]:BANDA_DRON[1]]
    return round(sum(int(c) for c in trozo) / len(trozo), 2)


def revisar_alerta(v: Ventana, nivel: Optional[float]) -> tuple[bool, Optional[str]]:
    """Regla simple y provisional de 'posible dron'."""
    motivos = []
    if v.tramas_dji > 0:
        motivos.append(f"{v.tramas_dji} tramas de un aparato DJI")
    if nivel is not None and nivel >= UMBRAL_BANDA:
        motivos.append(f"banda del dron ocupada (nivel {nivel} >= {UMBRAL_BANDA})")
    return (bool(motivos), "; ".join(motivos) or None)


# ---------------------------------------------------------------- CSV
_candado_csv = threading.Lock()


def guardar_csv(nombre: str, fila: dict):
    CARPETA_DATOS.mkdir(parents=True, exist_ok=True)
    ruta = CARPETA_DATOS / f"{nombre}.csv"
    with _candado_csv:
        nuevo = not ruta.exists()
        with ruta.open("a", newline="", encoding="utf-8") as f:
            w = csv.DictWriter(f, fieldnames=list(fila.keys()))
            if nuevo:
                w.writeheader()
            w.writerow(fila)


def insertar(tabla: str, fila: dict):
    columnas = ", ".join(fila)
    marcas = ", ".join(f"%({k})s" for k in fila)
    try:
        with bd() as con:
            con.execute(f"INSERT INTO {tabla} ({columnas}) VALUES ({marcas})", fila)
    except psycopg.Error as e:
        print(f"   !! No se pudo guardar en PostgreSQL: {e}", flush=True)
        raise HTTPException(503, "No se pudo guardar en la base de datos")


def guardar(tabla: str, datos: dict, request: Request):
    fila = {
        "recibido_en": datetime.now(timezone.utc).astimezone().isoformat(timespec="seconds"),
        "ip_origen": request.client.host if request.client else None,
        **datos,
    }
    guardar_csv(tabla, fila)
    insertar(tabla, fila)
    return fila


def hora():
    return datetime.now().strftime("%H:%M:%S")


# ---------------------------------------------------------------- aplicacion
@asynccontextmanager
async def ciclo_de_vida(_app):
    crear_tablas()
    yield


app = FastAPI(title="Detector de drones — Practica 1", version="1.0", lifespan=ciclo_de_vida)


@app.post("/api/latido")
def recibir_latido(latido: Latido, request: Request):
    guardar("latidos", latido.model_dump(), request)
    senal = f"senal {latido.rssi} dBm" if latido.rssi is not None else "por cable (sin WiFi propio)"
    print(f"[{hora()}] latido   {latido.nodo} #{latido.contador}  {senal}", flush=True)
    return {"ok": True}


@app.post("/api/ventanas")
def recibir_ventana(v: Ventana, request: Request):
    nivel = nivel_banda(v.espectro)
    alerta, motivo = revisar_alerta(v, nivel)
    datos = v.model_dump() | {"nivel_banda_dron": nivel, "alerta": alerta, "motivo_alerta": motivo}
    guardar("ventanas", datos, request)
    aviso = f"  <<< POSIBLE DRON: {motivo}" if alerta else ""
    print(f"[{hora()}] ventana  {v.nodo} #{v.ventana}  {v.tramas} tramas, {v.aparatos} aparatos, "
          f"banda {nivel if nivel is not None else '-'}{aviso}", flush=True)
    return {"ok": True, "alerta": alerta, "motivo": motivo}


@app.post("/api/eventos")
def recibir_evento(e: Evento, request: Request):
    guardar("eventos", e.model_dump(), request)
    extra = " ".join(str(x) for x in (e.clase, e.probabilidad, e.detalle) if x is not None)
    print(f"[{hora()}] EVENTO   {e.nodo} {e.tipo}  {extra}", flush=True)
    return {"ok": True}


@app.get("/api/resumen")
def resumen():
    with bd() as con:
        nodos = con.execute("""
            SELECT DISTINCT ON (nodo) nodo, recibido_en, contador, rssi,
                   EXTRACT(EPOCH FROM now() - recibido_en)::int AS hace_s
            FROM latidos WHERE recibido_en > now() - interval '1 hour'
            ORDER BY nodo, recibido_en DESC""").fetchall()
        ventanas = con.execute("""
            SELECT recibido_en, nodo, ventana, canal, tramas, aparatos, rssi_prom, rssi_max,
                   tramas_dji, espectro, nivel_banda_dron, alerta, motivo_alerta
            FROM ventanas ORDER BY recibido_en DESC, id DESC LIMIT 20""").fetchall()
        eventos = con.execute("""
            SELECT recibido_en, nodo, tipo, clase, probabilidad, detalle
            FROM eventos ORDER BY recibido_en DESC, id DESC LIMIT 10""").fetchall()
        totales = con.execute("""
            SELECT (SELECT count(*) FROM latidos) AS latidos,
                   (SELECT count(*) FROM ventanas) AS ventanas,
                   (SELECT count(*) FROM eventos) AS eventos,
                   (SELECT count(*) FROM ventanas
                     WHERE alerta AND recibido_en > now() - interval '1 hour') AS alertas_hora""").fetchone()
    return {"nodos": nodos, "ventanas": ventanas, "eventos": eventos, "totales": totales,
            "umbral_banda": UMBRAL_BANDA}


@app.get("/")
def pagina():
    return FileResponse(CARPETA / "panel.html")


# ---------------------------------------------------------------- arranque
def ip_del_wifi(ips):
    """Elige la IP del PC en la red de la casa (192.168.x, luego 10.x, luego 172.16-31.x)."""
    for prefijo in ("192.168.", "10.", "172."):
        for ip in ips:
            if ip.startswith(prefijo):
                return ip
    return None


def ips_locales():
    try:
        salida = subprocess.run(["hostname", "-I"], capture_output=True, text=True).stdout.split()
    except Exception:
        salida = []
    return [ip for ip in salida if ip.count(".") == 3]


if __name__ == "__main__":
    import uvicorn

    try:
        crear_tablas()
    except psycopg.Error as e:
        print("No pude conectarme a PostgreSQL. Revisa que este encendido y el archivo .env.")
        print("Detalle:", e)
        raise SystemExit(1)

    ip_pc = ip_del_wifi(ips_locales())
    print("=" * 64)
    print(f" Servidor del detector escuchando en el puerto {PUERTO}")
    print(f" Base de datos: OK   |   CSV en: {CARPETA_DATOS}")
    print(f" Pagina en este PC:        http://localhost:{PUERTO}")
    if ip_pc:
        print(f" IP de este PC en tu WiFi: {ip_pc}   (ponla en secrets.h del devkit)")
        print(f" Pagina desde el celular:  http://{ip_pc}:{PUERTO}")
    print(" Ctrl+C para salir")
    print("=" * 64, flush=True)
    uvicorn.run(app, host="0.0.0.0", port=PUERTO, access_log=False, log_level="warning")
