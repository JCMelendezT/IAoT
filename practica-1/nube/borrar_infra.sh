#!/usr/bin/env bash
# Borra TODO lo de la Practica 1 en Azure (el grupo rg-iaot-p1 completo).
# Hazlo cuando termines de presentar: la IP publica estatica cobra aunque la VM este apagada.
set -euo pipefail
GRUPO="rg-iaot-p1"
read -r -p "Esto borra el grupo $GRUPO y todo lo que tiene. Escribe BORRAR para seguir: " OK
[ "$OK" = "BORRAR" ] || { echo "Cancelado."; exit 1; }
az group delete -n "$GRUPO" --yes --no-wait
echo "Borrando en segundo plano. Revisa en el portal que el grupo desaparezca."
