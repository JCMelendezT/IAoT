"""Servidor MCP del detector: solo lectura del historial de eventos.

Habla con la API del devkit_centinela y no cambia nada en el detector.
La dirección se toma de DETECTOR_URL (por defecto http://detector.local).
"""
import os
from datetime import datetime, timedelta, timezone

import httpx
from mcp.server.fastmcp import FastMCP

URL = os.environ.get("DETECTOR_URL", "http://detector.local").rstrip("/")
ZONA = timezone(timedelta(hours=-5))          # hora de Colombia
ETIQUETAS = {0: "sin etiqueta", 1: "fondo", 2: "bluetooth", 3: "dron"}

mcp = FastMCP("detector-drones")


def pedir(ruta: str, **params) -> dict:
    try:
        r = httpx.get(URL + ruta, params=params, timeout=15)
        r.raise_for_status()
        return r.json()
    except httpx.HTTPError as e:
        raise RuntimeError(
            f"No pude hablar con el detector en {URL}: {e}. "
            "Revisa que el PC y el devkit estén en la misma red y la IP en DETECTOR_URL."
        ) from e


def hora(epoch: int, con_dia: bool = True) -> str:
    return datetime.fromtimestamp(epoch, ZONA).strftime("%Y-%m-%d %H:%M" if con_dia else "%H:%M")


@mcp.tool()
def eventos_recientes() -> list[dict]:
    """Los últimos eventos que anotó el devkit (máximo 10, del más nuevo al más viejo):
    alertas, cambios de WiFi, grabaciones. Se pierden si el devkit se reinicia."""
    estado = pedir("/api/estado")
    return [
        {"hora": hora(e["epoch"]) if e["epoch"] else "sin hora", "texto": e["texto"]}
        for e in estado.get("eventos", [])
    ]


@mcp.tool()
def historial_alertas(horas: int = 12) -> dict:
    """Resume las alertas de las últimas horas (1 a 24) a partir de la caja negra del devkit.
    Une los minutos seguidos con alerta en un solo episodio y dice cuándo empezó, cuánto duró,
    la probabilidad máxima de dron y qué etiqueta tenían (fondo, bluetooth, dron o ninguna).
    Sirve para encontrar falsas alarmas, por ejemplo las de la noche."""
    horas = max(1, min(24, horas))
    h = pedir("/api/historial", horas=horas)
    inicio, minutos = h["inicio"], h["min"]

    episodios, actual = [], None
    for i, (cuenta, dron, alerta, maxp, etiq) in enumerate(minutos):
        if alerta > 0:
            if actual is None:
                actual = {"i0": i, "i1": i, "p": maxp, "ventanas_alerta": 0, "etiquetas": {}}
            actual["i1"] = i
            actual["p"] = max(actual["p"], maxp)
            actual["ventanas_alerta"] += alerta
            nombre = ETIQUETAS.get(etiq, "?")
            actual["etiquetas"][nombre] = actual["etiquetas"].get(nombre, 0) + 1
        elif actual is not None:
            episodios.append(actual)
            actual = None
    if actual is not None:
        episodios.append(actual)

    salida = []
    for e in episodios:
        t0 = inicio + e["i0"] * 60
        salida.append({
            "desde": hora(t0),
            "hasta": hora(inicio + e["i1"] * 60 + 60, con_dia=False),
            "minutos": e["i1"] - e["i0"] + 1,
            "ventanas_con_alerta": e["ventanas_alerta"],
            "prob_dron_maxima": round(e["p"] / 255, 2),
            "etiquetas": e["etiquetas"],
        })

    con_datos = sum(1 for m in minutos if m[0] > 0)
    return {
        "periodo": f"{hora(inicio)} a {hora(inicio + len(minutos) * 60)}",
        "minutos_con_datos": con_datos,
        "minutos_sin_datos": len(minutos) - con_datos,
        "episodios_de_alerta": len(salida),
        "episodios": salida,
        "nota": "prob_dron_maxima va de 0 a 1. Un episodio sin etiqueta todavía no está revisado.",
    }


if __name__ == "__main__":
    mcp.run()
