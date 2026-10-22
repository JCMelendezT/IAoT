#!/usr/bin/env bash
# Enciende el servidor de la Practica 1:   bash arrancar.sh
cd "$(dirname "$0")"
VENV="$HOME/.venvs/iaot-p1"
if [ ! -x "$VENV/bin/python" ] || [ ! -f .env ]; then
  echo "Falta preparar Ubuntu. Corre primero:  bash preparar_ubuntu.sh"; exit 1
fi
if ! pg_isready -q -h localhost; then
  echo "Encendiendo PostgreSQL (puede pedir tu clave de Ubuntu)..."
  sudo service postgresql start
  sleep 2
fi
exec "$VENV/bin/python" app.py
