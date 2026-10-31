#!/usr/bin/env bash
# Apaga lo que cobra por hora: vm-influx, vm-iaot y (si todavia existe) PostgreSQL de la Practica 1.
# Siguen cobrando los discos y la IP estatica.
set -euo pipefail
GRUPO="rg-iaot-p1"
az vm deallocate -g "$GRUPO" -n vm-influx -o none --no-wait
echo "vm-influx apagandose."
az vm deallocate -g "$GRUPO" -n vm-iaot -o none
echo "vm-iaot apagada."
PG="$(az postgres flexible-server list -g "$GRUPO" --query "[0].name" -o tsv 2>/dev/null || true)"
if [ -n "$PG" ]; then
  az postgres flexible-server stop -g "$GRUPO" -n "$PG" -o none 2>/dev/null || true
  echo "PostgreSQL $PG detenido (Azure lo vuelve a encender solo a los 7 dias)."
fi
echo "Listo. Para volver: bash reanudar_p2.sh"
