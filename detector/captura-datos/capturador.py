"""
Capturador del detector de drones.

Ventana sencilla para grabar sesiones de datos con el ESP32-S3 (firmware nodo_captura):
  1. Elige el puerto y dale "Conectar". Veras la cascada en vivo.
  2. Elige la clase, la distancia, el estado y el escenario.
  3. Dale "GRABAR". Tienes 5 segundos para alejarte; luego graba la duracion elegida.
  4. Al terminar guarda el archivo en raw/ y anota la sesion en sesiones.csv.

Requiere: Python 3 y pyserial (Capturar.bat lo instala si falta).
"""

import csv
import os
import queue
import re
import threading
import time
from collections import deque
from datetime import datetime

import tkinter as tk
from tkinter import messagebox, ttk

try:
    import serial
    import serial.tools.list_ports
except ImportError:  # el .bat lo instala; esto es por si se abre directo
    serial = None

CARPETA = os.path.dirname(os.path.abspath(__file__))
CARPETA_RAW = os.path.join(CARPETA, "raw")
ARCHIVO_SESIONES = os.path.join(CARPETA, "sesiones.csv")
COLUMNAS_SESIONES = ["sesion_id", "fecha", "clase", "estado", "distancia_m",
                     "escenario", "sensor", "archivo_raw", "notas"]

CLASES = [
    ("fondo", "Fondo: la casa normal, sin nada especial"),
    ("celular-video", "Celular viendo video cerca"),
    ("bluetooth", "Audifonos o parlante Bluetooth sonando"),
    ("dron-control", "Dron con su control (en 2,4 GHz)"),
]
ESTADOS = ["na", "tierra", "vuelo"]
DISTANCIAS = ["0", "1", "3", "5", "10", "20", "30"]
ESCENARIOS = ["cuarto", "pasillo", "patio", "casa"]
CUENTA_REGRESIVA_S = 5
FILAS_CASCADA = 80
CANALES = 126
ZOOM_X, ZOOM_Y = 4, 3
# 0 = nada ... 8 = ocupado siempre
COLORES = ["#000000", "#1b0c41", "#4a0c6b", "#781c6d", "#a52c60",
           "#cf4446", "#ed6925", "#fb9b06", "#fcffa4"]

CAMPOS_W = ["ms", "ventana", "canal", "tramas", "aparatos", "mgmt", "datos", "tam_prom",
            "rssi_prom", "rssi_max", "tramas_debiles", "aparatos_fuertes",
            "tramas_dji", "rssi_max_dji"]


# ----------------------------------------------------------------------
# Funciones puras (sin ventana): faciles de probar
# ----------------------------------------------------------------------
def limpiar_texto(texto):
    """minusculas, sin espacios ni simbolos raros: apto para nombre de archivo."""
    texto = texto.strip().lower().replace(" ", "-")
    return re.sub(r"[^a-z0-9\-]", "", texto) or "sin-nombre"


def nombre_sesion(ahora, clase, estado, distancia, escenario):
    return "{}_{}-{}_{}m_{}_{}".format(
        ahora.strftime("%Y%m%d"), clase, estado, distancia,
        limpiar_texto(escenario), ahora.strftime("%H%M%S"))


def leer_fila_cascada(linea):
    """'N,<ms>,<126 digitos>' -> lista de 126 enteros 0..8, o None."""
    partes = linea.split(",", 2)
    if len(partes) != 3 or partes[0] != "N":
        return None
    digitos = partes[2].strip()
    if len(digitos) != CANALES or not digitos.isdigit():
        return None
    return [min(int(d), 8) for d in digitos]


def leer_ventana_wifi(linea):
    """'W,...' -> diccionario con los campos, o None."""
    partes = linea.split(",")
    if len(partes) != len(CAMPOS_W) + 1 or partes[0] != "W":
        return None
    try:
        return dict(zip(CAMPOS_W, (int(x) for x in partes[1:])))
    except ValueError:
        return None


def anotar_sesion(fila, archivo=ARCHIVO_SESIONES):
    nuevo = not os.path.exists(archivo) or os.path.getsize(archivo) == 0
    with open(archivo, "a", newline="", encoding="utf-8") as f:
        w = csv.DictWriter(f, fieldnames=COLUMNAS_SESIONES)
        if nuevo:
            w.writeheader()
        w.writerow(fila)


# ----------------------------------------------------------------------
# Lectura del puerto serie en segundo plano
# ----------------------------------------------------------------------
class LectorSerie(threading.Thread):
    def __init__(self, puerto, cola):
        super().__init__(daemon=True)
        self.puerto, self.cola = puerto, cola
        self.activo = True
        self.ser = None

    def run(self):
        try:
            self.ser = serial.Serial(self.puerto, 115200, timeout=0.5)
        except Exception as e:  # puerto ocupado o inexistente
            self.cola.put(("error", "No pude abrir {}: {}".format(self.puerto, e)))
            return
        self.cola.put(("ok", "Conectado a {}".format(self.puerto)))
        while self.activo:
            try:
                crudo = self.ser.readline()
            except Exception as e:
                self.cola.put(("error", "Se perdio la conexion: {}".format(e)))
                break
            if crudo:
                self.cola.put(("linea", crudo.decode("utf-8", errors="ignore").strip()))
        try:
            self.ser.close()
        except Exception:
            pass

    def detener(self):
        self.activo = False


# ----------------------------------------------------------------------
# Ventana
# ----------------------------------------------------------------------
class App:
    def __init__(self, raiz):
        self.raiz = raiz
        raiz.title("Capturador - Detector de drones")
        raiz.resizable(False, False)
        self.cola = queue.Queue()
        self.lector = None
        self.cascada = deque(maxlen=FILAS_CASCADA)
        self.cascada_cambio = False
        self.estado = "parado"  # parado | cuenta | grabando
        self.archivo = None
        self.sesion = None
        self.t_inicio = 0.0
        self.t_fin = 0.0
        self.n_filas = 0
        self.n_ventanas = 0
        self.imagen = None

        fuente = ("Segoe UI", 11)
        grande = ("Segoe UI", 16, "bold")
        marco = ttk.Frame(raiz, padding=12)
        marco.grid(sticky="nsew")

        # --- Conexion ---
        con = ttk.LabelFrame(marco, text="1. Conexion", padding=8)
        con.grid(row=0, column=0, sticky="ew", pady=4)
        ttk.Label(con, text="Puerto:", font=fuente).grid(row=0, column=0)
        self.var_puerto = tk.StringVar()
        self.combo_puerto = ttk.Combobox(con, textvariable=self.var_puerto, width=12, state="readonly")
        self.combo_puerto.grid(row=0, column=1, padx=4)
        ttk.Button(con, text="Actualizar", command=self.listar_puertos).grid(row=0, column=2, padx=4)
        self.boton_conectar = ttk.Button(con, text="Conectar", command=self.conectar)
        self.boton_conectar.grid(row=0, column=3, padx=4)
        self.lbl_conexion = ttk.Label(con, text="Sin conectar", font=fuente, foreground="gray")
        self.lbl_conexion.grid(row=0, column=4, padx=8)

        # --- Etiquetas de la sesion ---
        eti = ttk.LabelFrame(marco, text="2. Que vas a grabar", padding=8)
        eti.grid(row=1, column=0, sticky="ew", pady=4)
        self.var_clase = tk.StringVar(value=CLASES[0][0])
        for i, (clave, texto) in enumerate(CLASES):
            ttk.Radiobutton(eti, text=texto, value=clave, variable=self.var_clase,
                            command=self.al_cambiar_clase).grid(row=i, column=0, columnspan=4, sticky="w")
        fila = len(CLASES)
        ttk.Label(eti, text="Estado:", font=fuente).grid(row=fila, column=0, sticky="e", pady=(8, 0))
        self.var_estado = tk.StringVar(value="na")
        self.combo_estado = ttk.Combobox(eti, textvariable=self.var_estado, values=ESTADOS,
                                         width=8, state="disabled")
        self.combo_estado.grid(row=fila, column=1, sticky="w", pady=(8, 0))
        ttk.Label(eti, text="Distancia (m):", font=fuente).grid(row=fila, column=2, sticky="e", pady=(8, 0))
        self.var_dist = tk.StringVar(value="1")
        ttk.Combobox(eti, textvariable=self.var_dist, values=DISTANCIAS, width=6).grid(
            row=fila, column=3, sticky="w", pady=(8, 0))
        ttk.Label(eti, text="Escenario:", font=fuente).grid(row=fila + 1, column=0, sticky="e")
        self.var_esc = tk.StringVar(value="cuarto")
        ttk.Combobox(eti, textvariable=self.var_esc, values=ESCENARIOS, width=10).grid(
            row=fila + 1, column=1, sticky="w")
        ttk.Label(eti, text="Duracion (s):", font=fuente).grid(row=fila + 1, column=2, sticky="e")
        self.var_dur = tk.StringVar(value="60")
        ttk.Spinbox(eti, from_=10, to=600, increment=10, textvariable=self.var_dur, width=6).grid(
            row=fila + 1, column=3, sticky="w")
        ttk.Label(eti, text="Notas:", font=fuente).grid(row=fila + 2, column=0, sticky="e")
        self.var_notas = tk.StringVar()
        ttk.Entry(eti, textvariable=self.var_notas, width=50).grid(row=fila + 2, column=1, columnspan=3, sticky="w")

        # --- Grabar ---
        gra = ttk.LabelFrame(marco, text="3. Grabar", padding=8)
        gra.grid(row=2, column=0, sticky="ew", pady=4)
        self.boton_grabar = tk.Button(gra, text="●  GRABAR", font=grande, bg="#c62828", fg="white",
                                      activebackground="#8e0000", width=12, command=self.grabar)
        self.boton_grabar.grid(row=0, column=0, padx=4)
        self.boton_detener = tk.Button(gra, text="■  Detener", font=grande, width=10,
                                       command=self.detener_grabacion, state="disabled")
        self.boton_detener.grid(row=0, column=1, padx=4)
        ttk.Button(gra, text="Abrir carpeta de datos", command=self.abrir_carpeta).grid(row=0, column=2, padx=8)
        self.lbl_estado = tk.Label(gra, text="Conecta el ESP32 para empezar", font=grande, fg="#555")
        self.lbl_estado.grid(row=1, column=0, columnspan=3, pady=(8, 0), sticky="w")

        # --- Vista en vivo ---
        viv = ttk.LabelFrame(marco, text="En vivo", padding=8)
        viv.grid(row=3, column=0, sticky="ew", pady=4)
        ancho, alto = CANALES * ZOOM_X, FILAS_CASCADA * ZOOM_Y
        self.lienzo = tk.Canvas(viv, width=ancho, height=alto, bg="black", highlightthickness=0)
        self.lienzo.grid(row=0, column=0, columnspan=3)
        self.item_imagen = self.lienzo.create_image(0, 0, anchor="nw")
        for canal_nrf, nombre in ((12, "WiFi 1"), (37, "WiFi 6"), (62, "WiFi 11")):
            x = canal_nrf * ZOOM_X + ZOOM_X // 2
            self.lienzo.create_line(x, 0, x, 6, fill="white")
            self.lienzo.create_text(x + 3, 8, text=nombre, fill="white", anchor="nw", font=("Segoe UI", 8))
        ttk.Label(viv, text="2400 MHz").grid(row=1, column=0, sticky="w")
        ttk.Label(viv, text="Espectro (arriba = ahora)").grid(row=1, column=1)
        ttk.Label(viv, text="2525 MHz").grid(row=1, column=2, sticky="e")
        self.lbl_wifi = ttk.Label(viv, text="WiFi: esperando datos...", font=fuente)
        self.lbl_wifi.grid(row=2, column=0, columnspan=3, sticky="w", pady=(6, 0))

        self.listar_puertos()
        self.raiz.protocol("WM_DELETE_WINDOW", self.cerrar)
        self.raiz.after(50, self.ciclo)

    # ----- conexion -----
    def listar_puertos(self):
        if serial is None:
            self.lbl_conexion.config(text="Falta pyserial: abre con Capturar.bat", foreground="red")
            return
        puertos = sorted(p.device for p in serial.tools.list_ports.comports())
        self.combo_puerto["values"] = puertos
        if puertos and self.var_puerto.get() not in puertos:
            self.var_puerto.set("COM11" if "COM11" in puertos else puertos[0])

    def conectar(self):
        if self.lector:
            self.lector.detener()
            self.lector = None
            self.boton_conectar.config(text="Conectar")
            self.lbl_conexion.config(text="Sin conectar", foreground="gray")
            return
        if not self.var_puerto.get():
            messagebox.showwarning("Puerto", "No encuentro ningun puerto. Conecta el ESP32 y dale Actualizar.")
            return
        self.lector = LectorSerie(self.var_puerto.get(), self.cola)
        self.lector.start()
        self.boton_conectar.config(text="Desconectar")
        self.lbl_conexion.config(text="Conectando...", foreground="orange")

    # ----- etiquetas -----
    def al_cambiar_clase(self):
        if self.var_clase.get() == "dron-control":
            self.combo_estado.config(state="readonly")
            if self.var_estado.get() == "na":
                self.var_estado.set("tierra")
        else:
            self.var_estado.set("na")
            self.combo_estado.config(state="disabled")

    # ----- grabacion -----
    def grabar(self):
        if self.estado != "parado":
            return
        if not self.lector:
            messagebox.showwarning("Sin conexion", "Primero conecta el ESP32 (paso 1).")
            return
        try:
            duracion = int(self.var_dur.get())
            assert duracion > 0
        except (ValueError, AssertionError):
            messagebox.showwarning("Duracion", "La duracion debe ser un numero de segundos.")
            return
        if self.var_clase.get() == "dron-control" and not messagebox.askyesno(
                "Revisar el dron",
                "Antes de grabar:\n\n"
                "- El dron esta con su CONTROL?\n"
                "- En DJI Fly > Trans. > Frecuencia esta en 2.4 GHz?\n"
                "- Protectores de helice y nadie cerca?\n\n"
                "Todo listo?"):
            return
        self.duracion = duracion
        self.estado = "cuenta"
        self.t_inicio = time.time() + CUENTA_REGRESIVA_S
        self.boton_grabar.config(state="disabled")
        self.boton_detener.config(state="normal")

    def empezar_archivo(self):
        os.makedirs(CARPETA_RAW, exist_ok=True)
        ahora = datetime.now()
        clase, estado = self.var_clase.get(), self.var_estado.get()
        self.sesion = {
            "sesion_id": nombre_sesion(ahora, clase, estado, self.var_dist.get(), self.var_esc.get()),
            "fecha": ahora.strftime("%Y-%m-%d"),
            "clase": clase,
            "estado": estado,
            "distancia_m": self.var_dist.get(),
            "escenario": limpiar_texto(self.var_esc.get()),
            "sensor": "nodo-captura",
            "notas": self.var_notas.get().strip(),
        }
        self.sesion["archivo_raw"] = "raw/{}.txt".format(self.sesion["sesion_id"])
        ruta = os.path.join(CARPETA, self.sesion["archivo_raw"])
        self.archivo = open(ruta, "w", encoding="utf-8")
        self.archivo.write("# sesion: {}\n".format(self.sesion["sesion_id"]))
        for k in ("fecha", "clase", "estado", "distancia_m", "escenario", "notas"):
            self.archivo.write("# {}: {}\n".format(k, self.sesion[k]))
        self.archivo.write("# inicio: {}\n".format(ahora.isoformat(timespec="seconds")))
        self.archivo.write("# formato: <segundos desde el inicio> <linea del ESP32>\n")
        self.t_inicio = time.time()
        self.t_fin = self.t_inicio + self.duracion
        self.n_filas = self.n_ventanas = 0
        self.estado = "grabando"

    def detener_grabacion(self, completa=False):
        if self.estado == "cuenta":
            self.estado = "parado"
            self.lbl_estado.config(text="Cancelado", fg="#555")
        elif self.estado == "grabando":
            self.estado = "parado"
            self.archivo.write("# fin: {} ({})\n".format(
                datetime.now().isoformat(timespec="seconds"), "completa" if completa else "detenida a mano"))
            self.archivo.close()
            self.archivo = None
            if not completa:
                self.sesion["notas"] = (self.sesion["notas"] + " [detenida a mano]").strip()
            anotar_sesion(self.sesion)
            self.lbl_estado.config(
                text="Guardado: {} ({} filas, {} ventanas)".format(
                    self.sesion["sesion_id"], self.n_filas, self.n_ventanas),
                fg="#2e7d32")
            if self.n_filas == 0:
                messagebox.showwarning("Ojo", "No llego ninguna fila del nRF24. Revisa el modulo.")
        self.boton_grabar.config(state="normal")
        self.boton_detener.config(state="disabled")

    def abrir_carpeta(self):
        os.makedirs(CARPETA_RAW, exist_ok=True)
        try:
            os.startfile(CARPETA_RAW)  # Windows
        except AttributeError:
            messagebox.showinfo("Carpeta", CARPETA_RAW)

    # ----- ciclo de la ventana -----
    def ciclo(self):
        try:
            while True:
                tipo, dato = self.cola.get_nowait()
                if tipo == "linea":
                    self.procesar_linea(dato)
                elif tipo == "ok":
                    self.lbl_conexion.config(text=dato, foreground="green")
                    if self.estado == "parado":
                        self.lbl_estado.config(text="Listo para grabar", fg="#555")
                elif tipo == "error":
                    self.lbl_conexion.config(text=dato, foreground="red")
                    self.lector = None
                    self.boton_conectar.config(text="Conectar")
                    if self.estado != "parado":
                        self.detener_grabacion()
        except queue.Empty:
            pass

        ahora = time.time()
        if self.estado == "cuenta":
            faltan = int(self.t_inicio - ahora) + 1
            if ahora >= self.t_inicio:
                self.empezar_archivo()
            else:
                self.lbl_estado.config(text="Empieza en {}... alejate del equipo".format(faltan), fg="#e65100")
        if self.estado == "grabando":
            if ahora >= self.t_fin:
                self.detener_grabacion(completa=True)
            else:
                self.lbl_estado.config(text="GRABANDO {}  {} / {} s".format(
                    self.var_clase.get(), int(ahora - self.t_inicio), self.duracion), fg="#c62828")

        if self.cascada_cambio:
            self.dibujar_cascada()
        self.raiz.after(50, self.ciclo)

    def procesar_linea(self, linea):
        if self.estado == "grabando" and self.archivo:
            self.archivo.write("{:.3f} {}\n".format(time.time() - self.t_inicio, linea))
        fila = leer_fila_cascada(linea)
        if fila is not None:
            self.cascada.appendleft(fila)
            self.cascada_cambio = True
            if self.estado == "grabando":
                self.n_filas += 1
            return
        w = leer_ventana_wifi(linea)
        if w is not None:
            if self.estado == "grabando":
                self.n_ventanas += 1
            self.lbl_wifi.config(text="WiFi (ultimos 2 s): {} paquetes, {} aparatos, senal max {} dBm{}".format(
                w["tramas"], w["aparatos"], w["rssi_max"],
                ", DJI: {} paquetes".format(w["tramas_dji"]) if w["tramas_dji"] else ""))
            return
        if linea.startswith("# ERROR"):
            self.lbl_conexion.config(text=linea[2:], foreground="red")

    def dibujar_cascada(self):
        self.cascada_cambio = False
        base = tk.PhotoImage(width=CANALES, height=FILAS_CASCADA)
        filas = list(self.cascada) + [[0] * CANALES] * (FILAS_CASCADA - len(self.cascada))
        datos = " ".join("{" + " ".join(COLORES[v] for v in fila) + "}" for fila in filas)
        base.put(datos)
        self.imagen = base.zoom(ZOOM_X, ZOOM_Y)
        self.lienzo.itemconfig(self.item_imagen, image=self.imagen)

    def cerrar(self):
        if self.estado == "grabando":
            if not messagebox.askyesno("Salir", "Hay una grabacion en curso. Guardarla y salir?"):
                return
            self.detener_grabacion()
        if self.lector:
            self.lector.detener()
        self.raiz.destroy()


if __name__ == "__main__":
    raiz = tk.Tk()
    App(raiz)
    raiz.mainloop()
