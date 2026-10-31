#!/usr/bin/env bash
# Enciende todo para trabajar en la Practica 2: vm-influx primero (la base), luego
# vm-iaot (broker, puente y Grafana). PostgreSQL de la Practica 1 NO se enciende
# (no se usa en la Practica 2 y asi no cobra).
set -euo pipefail
GRUPO="rg-iaot-p1"
cd "$(dirname "$0")"
source ../../practica-1/nube/infra.env
az vm start -g "$GRUPO" -n vm-influx -o none
echo "vm-influx encendida."
az vm start -g "$GRUPO" -n vm-iaot -o none
echo "vm-iaot encendida. Esperando a los servicios..."
sleep 40
ssh -o ConnectTimeout=15 "$USUARIO_VM@$IP_PUBLICA" \
  "for s in mosquitto puente-mqtt grafana-server; do printf '  %-15s %s\n' \$s \$(systemctl is-active \$s); done"
echo "Grafana: http://$IP_PUBLICA:3000    Broker: $IP_PUBLICA:1883"
