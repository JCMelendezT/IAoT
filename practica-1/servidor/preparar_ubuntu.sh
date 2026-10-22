#!/usr/bin/env bash
# Prepara Ubuntu (WSL) para el servidor de la Practica 1. Se corre UNA sola vez:
#     bash preparar_ubuntu.sh
# Te pedira la clave de tu usuario de Ubuntu (para instalar) y una clave nueva para la base.
set -e
cd "$(dirname "$0")"
VENV="$HOME/.venvs/iaot-p1"

echo
echo "== Paso 1 de 4: instalando PostgreSQL y Python (puede pedir tu clave de Ubuntu) =="
sudo apt-get update -y
sudo apt-get install -y postgresql python3-venv python3-pip

echo
echo "== Paso 2 de 4: encendiendo PostgreSQL =="
sudo service postgresql start
sleep 2

echo
echo "== Paso 3 de 4: creando la base de datos 'iaot' =="
if [ -f .env ]; then
  echo "Ya existe un archivo .env: uso esa clave y no creo otra."
  CLAVE="$(grep '^DB_CLAVE=' .env | cut -d= -f2-)"
else
  while true; do
    read -r -s -p "Inventa una clave para la base (solo letras y numeros, minimo 8; no se ve al escribir): " CLAVE; echo
    if [[ "$CLAVE" =~ ^[A-Za-z0-9]{8,}$ ]]; then break; fi
    echo "  Esa clave no sirve: usa solo letras y numeros, minimo 8."
  done
fi
(
  cd /tmp
  sudo -u postgres psql -q -v ON_ERROR_STOP=1 -c \
    "DO \$\$BEGIN IF NOT EXISTS (SELECT FROM pg_roles WHERE rolname='iaot') THEN CREATE ROLE iaot LOGIN PASSWORD '$CLAVE'; ELSE ALTER ROLE iaot PASSWORD '$CLAVE'; END IF; END\$\$;"
  if ! sudo -u postgres psql -tAc "SELECT 1 FROM pg_database WHERE datname='iaot'" | grep -q 1; then
    sudo -u postgres createdb -O iaot iaot
  fi
)
if [ ! -f .env ]; then
  sed "s/^DB_CLAVE=.*/DB_CLAVE=$CLAVE/" .env.example > .env
  echo "Guarde la configuracion en .env (este archivo no se sube a GitHub)."
fi

echo
echo "== Paso 4 de 4: instalando las librerias de Python en $VENV =="
python3 -m venv "$VENV"
"$VENV/bin/pip" install --upgrade pip -q
"$VENV/bin/pip" install -r requirements.txt -q

echo
echo "LISTO. Para encender el servidor:  bash arrancar.sh"
