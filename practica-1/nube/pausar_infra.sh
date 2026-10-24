#!/usr/bin/env bash
# Apaga lo que cobra por hora: la VM y el computo de PostgreSQL.
# Sigue cobrando (por eso existe borrar_infra.sh):
#   - el disco de la VM       ~6,7 USD/mes
#   - la IP publica estatica  ~3,7 USD/mes
#   - el almacenamiento de la base (32 GB) ~5,2 USD/mes
# Precio con todo apagado: ~0,5 USD/dia. Borrar todo: 0.
set -euo pipefail
GRUPO="rg-iaot-p1"
SUFIJO="$(printf '%s' "$(az account show --query id -o tsv)" | md5sum | cut -c1-6)"
SERVIDOR_BD="pg-iaot-${SUFIJO}"

az vm deallocate -g "$GRUPO" -n vm-iaot -o none
echo "VM apagada (deallocated): deja de cobrar CPU y RAM."

# PostgreSQL solo aguanta 7 dias detenido; despues Azure lo reinicia solo.
az postgres flexible-server stop -g "$GRUPO" -n "$SERVIDOR_BD" --yes -o none 2>/dev/null \
  || az postgres flexible-server stop -g "$GRUPO" -n "$SERVIDOR_BD" -o none
echo "PostgreSQL detenido: deja de cobrar el computo (sigue el almacenamiento)."
