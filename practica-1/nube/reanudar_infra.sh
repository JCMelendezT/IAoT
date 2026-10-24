#!/usr/bin/env bash
# Vuelve a encender lo que apago pausar_infra.sh.
set -euo pipefail
GRUPO="rg-iaot-p1"
SUFIJO="$(printf '%s' "$(az account show --query id -o tsv)" | md5sum | cut -c1-6)"
SERVIDOR_BD="pg-iaot-${SUFIJO}"

az postgres flexible-server start -g "$GRUPO" -n "$SERVIDOR_BD" -o none
echo "PostgreSQL arriba (tarda 1-2 minutos en aceptar conexiones)."
az vm start -g "$GRUPO" -n vm-iaot -o none

IP="$(az network public-ip show -g "$GRUPO" -n ip-iaot --query ipAddress -o tsv)"
echo
echo "Servidor: http://$IP:8000"
echo "Si la pagina no responde: ssh juan@$IP 'sudo systemctl status servidor-iaot'"
