#!/usr/bin/env bash
# Apaga TODO lo de las practicas 1 y 2 que cobra por hora:
#   vm-influx, vm-iaot y el computo de PostgreSQL (Practica 1).
# Siguen cobrando los discos, la IP estatica y el almacenamiento de PostgreSQL.
set -euo pipefail
GRUPO="rg-iaot-p1"
cd "$(dirname "$0")"
az vm deallocate -g "$GRUPO" -n vm-influx -o none --no-wait
echo "vm-influx apagandose."
bash ../../practica-1/nube/pausar_infra.sh
