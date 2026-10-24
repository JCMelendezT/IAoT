#!/usr/bin/env bash
# Practica 1, parte 2: copia el servidor a la maquina virtual y lo deja corriendo
# como servicio (se enciende solo si la VM se reinicia).
#
# Requisitos: haber corrido crear_infra.sh (deja el archivo infra.env).
# Uso (en Ubuntu/WSL, desde esta carpeta):
#   bash desplegar_app.sh
set -euo pipefail
cd "$(dirname "$0")"
source infra.env

if [ -z "${CLAVE_BD:-}" ]; then
  read -r -s -p "Clave del administrador de la base (la misma de crear_infra.sh): " CLAVE_BD; echo
fi

DESTINO="$USUARIO_VM@$IP_PUBLICA"
SSH="ssh -o StrictHostKeyChecking=accept-new $DESTINO"

echo "== 1/4 Copiando el servidor a la VM =="
$SSH "mkdir -p ~/detector/servidor ~/detector/datos"
scp -o StrictHostKeyChecking=accept-new \
  ../servidor/app.py ../servidor/esquema.sql ../servidor/panel.html \
  ../servidor/requirements.txt servidor-detector.service \
  "$DESTINO:~/detector/servidor/"

echo "== 2/4 Configuracion (.env en la VM, solo lectura para el usuario) =="
$SSH "umask 077; cat > ~/detector/servidor/.env" <<EOF
DB_HOST=$HOST_BD
DB_PUERTO=5432
DB_NOMBRE=iaot
DB_USUARIO=$ADMIN_BD
DB_CLAVE=$CLAVE_BD
PUERTO=8000
UMBRAL_BANDA=2.5
EOF

echo "== 3/4 Instalando Python y librerias en la VM =="
$SSH "sudo apt-get update -y -q && sudo apt-get install -y -q python3-venv postgresql-client \
  && python3 -m venv ~/venv && ~/venv/bin/pip install -q -r ~/detector/servidor/requirements.txt"

echo "== 4/4 Servicio systemd =="
$SSH "sed 's/__USUARIO__/$USUARIO_VM/g' ~/detector/servidor/servidor-detector.service \
  | sudo tee /etc/systemd/system/servidor-detector.service >/dev/null \
  && sudo systemctl daemon-reload && sudo systemctl enable --now servidor-detector \
  && sleep 3 && systemctl --no-pager status servidor-detector | head -5"

echo
echo "Probando desde aqui: http://$IP_PUBLICA:8000/api/resumen"
curl -s --max-time 10 "http://$IP_PUBLICA:8000/api/resumen" | head -c 200; echo
echo
echo "LISTO. Pagina: http://$IP_PUBLICA:8000"
echo "En el devkit: SERVIDOR_IP \"$IP_PUBLICA\" en secrets.h y volver a subir devkit_pasarela."
