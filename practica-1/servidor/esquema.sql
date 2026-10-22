-- Tablas de la Practica 1 (detector de drones).
-- Se crean solas al arrancar el servidor; si ya existen, no se tocan.

-- "Estoy vivo" de cada placa.
CREATE TABLE IF NOT EXISTS latidos (
    id           BIGSERIAL PRIMARY KEY,
    recibido_en  TIMESTAMPTZ NOT NULL DEFAULT now(),
    ip_origen    TEXT,
    nodo         TEXT    NOT NULL,
    contador     BIGINT  NOT NULL,
    ms           BIGINT  NOT NULL,
    rssi         INTEGER           -- vacio = placa sin WiFi propio
);
ALTER TABLE latidos ALTER COLUMN rssi DROP NOT NULL;

-- Resumen de lo que el nodo escucho en una ventana de 2 segundos.
CREATE TABLE IF NOT EXISTS ventanas (
    id                BIGSERIAL PRIMARY KEY,
    recibido_en       TIMESTAMPTZ NOT NULL DEFAULT now(),
    ip_origen         TEXT,
    nodo              TEXT    NOT NULL,
    ms                BIGINT  NOT NULL,
    ventana           BIGINT  NOT NULL,
    canal             INTEGER NOT NULL,   -- 0 = saltando de canal; 1..13 = canal fijo
    tramas            INTEGER NOT NULL,
    aparatos          INTEGER NOT NULL,
    mgmt              INTEGER NOT NULL,
    datos             INTEGER NOT NULL,
    tam_prom          REAL    NOT NULL,
    rssi_prom         REAL    NOT NULL,
    rssi_max          INTEGER NOT NULL,
    tramas_debiles    INTEGER NOT NULL,
    aparatos_fuertes  INTEGER NOT NULL,
    tramas_dji        INTEGER NOT NULL,
    rssi_max_dji      INTEGER NOT NULL,   -- -127 = no hubo tramas DJI
    espectro          TEXT,               -- 126 digitos 0..8 (2400..2525 MHz), puede faltar
    nivel_banda_dron  REAL,               -- promedio del espectro entre 2423 y 2456 MHz
    alerta            BOOLEAN NOT NULL DEFAULT FALSE,
    motivo_alerta     TEXT
);
CREATE INDEX IF NOT EXISTS ventanas_recibido_idx ON ventanas (recibido_en DESC);

-- Eventos que decide el propio nodo (por ejemplo, su alerta local).
CREATE TABLE IF NOT EXISTS eventos (
    id            BIGSERIAL PRIMARY KEY,
    recibido_en   TIMESTAMPTZ NOT NULL DEFAULT now(),
    ip_origen     TEXT,
    nodo          TEXT   NOT NULL,
    ms            BIGINT NOT NULL,
    tipo          TEXT   NOT NULL,
    clase         TEXT,
    probabilidad  REAL,
    detalle       TEXT
);
CREATE INDEX IF NOT EXISTS eventos_recibido_idx ON eventos (recibido_en DESC);
