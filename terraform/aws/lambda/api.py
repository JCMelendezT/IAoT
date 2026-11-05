"""API del dashboard (Lambda + API Gateway HTTP).

  GET  /lecturas?minutos=60&limite=600   -> lecturas del nodo (JSON)
  GET  /csv?horas=24                     -> descarga CSV
  GET  /estado                           -> estado del actuador + ultima lectura
  POST /actuador   {"estado": "on|off"}  -> publica el comando en AWS IoT Core

Todas piden la clave (cabecera x-token o ?token=).
"""
import csv
import hmac
import io
import json
import os
import time
from decimal import Decimal

import boto3
from boto3.dynamodb.conditions import Key

dynamo = boto3.resource("dynamodb")
T_LECTURAS = dynamo.Table(os.environ["TABLA_LECTURAS"])
T_ESTADO = dynamo.Table(os.environ["TABLA_ESTADO"])
IOT = boto3.client("iot-data", endpoint_url="https://" + os.environ["IOT_ENDPOINT"])
NODO = os.environ["NODO"]
RAIZ = os.environ["RAIZ"]
TOKEN = os.environ["TOKEN"]

COLUMNAS_CSV = ["fecha_utc", "ts_ms", "nodo", "tema", "nivel_banda", "aparatos", "tramas",
                "rssi_max", "alerta", "temperatura", "humedad", "ventana", "canal",
                "tipo", "detalle"]


def _num(o):
    if isinstance(o, Decimal):
        return int(o) if o % 1 == 0 else float(o)
    raise TypeError(type(o))


def _json(codigo, cuerpo):
    return {
        "statusCode": codigo,
        "headers": {"content-type": "application/json"},
        "body": json.dumps(cuerpo, default=_num),
    }


def _entero(qs, nombre, defecto, minimo, maximo):
    try:
        v = int(qs.get(nombre, defecto))
    except (TypeError, ValueError):
        v = defecto
    return max(minimo, min(maximo, v))


def _consultar(desde_ms):
    """Todas las filas del nodo desde desde_ms (maneja paginacion de DynamoDB)."""
    filas, clave = [], None
    while True:
        args = {"KeyConditionExpression": Key("nodo").eq(NODO) & Key("ts").gte(desde_ms)}
        if clave:
            args["ExclusiveStartKey"] = clave
        r = T_LECTURAS.query(**args)
        filas.extend(r["Items"])
        clave = r.get("LastEvaluatedKey")
        if not clave:
            return filas


def _ultima():
    r = T_LECTURAS.query(KeyConditionExpression=Key("nodo").eq(NODO),
                         ScanIndexForward=False, Limit=20)
    for f in r["Items"]:
        if f.get("tema") == "lectura":
            return f
    return None


def lecturas(qs):
    minutos = _entero(qs, "minutos", 60, 1, 1440)
    limite = _entero(qs, "limite", 600, 10, 3000)
    desde = int(time.time() * 1000) - minutos * 60_000
    filas = _consultar(desde)
    lect = [f for f in filas if f.get("tema") == "lectura"]
    eventos = [f for f in filas if f.get("tema") == "evento"]
    if len(lect) > limite:                       # reducir para que la grafica no pese
        paso = -(-len(lect) // limite)
        lect = lect[::paso]
    return _json(200, {"nodo": NODO, "lecturas": lect, "eventos": eventos[-50:]})


def csv_descarga(qs):
    horas = _entero(qs, "horas", 24, 1, 48)
    desde = int(time.time() * 1000) - horas * 3_600_000
    salida = io.StringIO()
    w = csv.DictWriter(salida, fieldnames=COLUMNAS_CSV, extrasaction="ignore")
    w.writeheader()
    for f in _consultar(desde):
        fila = {k: (_num(v) if isinstance(v, Decimal) else v) for k, v in f.items()}
        fila["ts_ms"] = fila.get("ts")
        fila["fecha_utc"] = time.strftime("%Y-%m-%d %H:%M:%S", time.gmtime(int(f["ts"]) / 1000))
        w.writerow(fila)
    return {
        "statusCode": 200,
        "headers": {
            "content-type": "text/csv; charset=utf-8",
            "content-disposition": 'attachment; filename="detector_%s.csv"' % time.strftime("%Y%m%d_%H%M"),
        },
        "body": salida.getvalue(),
    }


def estado():
    act = T_ESTADO.get_item(Key={"nodo": "actuador"}).get("Item")
    return _json(200, {"actuador": act, "ultima_lectura": _ultima(), "ahora_ms": int(time.time() * 1000)})


def actuador(cuerpo):
    try:
        orden = json.loads(cuerpo or "{}").get("estado")
    except ValueError:
        orden = None
    if orden not in ("on", "off"):
        return _json(400, {"error": "estado debe ser 'on' u 'off'"})
    IOT.publish(
        topic=f"{RAIZ}/actuador/cmd",
        qos=1,
        payload=json.dumps({"estado": orden, "origen": "dashboard", "ts": int(time.time() * 1000)}),
    )
    return _json(200, {"ok": True, "enviado": orden})


def handler(event, _contexto):
    http = event["requestContext"]["http"]
    metodo, ruta = http["method"], event["rawPath"]
    qs = event.get("queryStringParameters") or {}
    cabeceras = {k.lower(): v for k, v in (event.get("headers") or {}).items()}
    clave = cabeceras.get("x-token") or qs.get("token") or ""
    if not hmac.compare_digest(clave.encode(), TOKEN.encode()):
        return _json(401, {"error": "clave incorrecta"})
    try:
        if metodo == "GET" and ruta == "/lecturas":
            return lecturas(qs)
        if metodo == "GET" and ruta == "/csv":
            return csv_descarga(qs)
        if metodo == "GET" and ruta == "/estado":
            return estado()
        if metodo == "POST" and ruta == "/actuador":
            return actuador(event.get("body"))
    except Exception as e:  # el dashboard muestra el mensaje
        return _json(500, {"error": str(e)})
    return _json(404, {"error": "ruta no encontrada"})
