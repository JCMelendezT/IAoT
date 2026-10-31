#!/usr/bin/env bash
# Agrega tu IP publica ACTUAL a la regla SSH (nsg-app / permitir-ssh-mi-ip)
# SIN quitar las que ya estan. Si existe la regla de Grafana (Practica 2), la iguala. Sirve cada vez que trabajes desde otro lugar.
#
# Uso (Ubuntu/WSL, con "az login" hecho):
#   bash agregar_mi_ip.sh              -> agrega la IP de donde estas ahora
#   bash agregar_mi_ip.sh 1.2.3.4      -> agrega esa IP concreta
#   bash agregar_mi_ip.sh --quitar 1.2.3.4   -> la quita de la lista
#   bash agregar_mi_ip.sh --ver        -> solo muestra la lista actual
set -euo pipefail
GRUPO="rg-iaot-p1"; NSG="nsg-app"; REGLA="permitir-ssh-mi-ip"

actuales() {
  {
    az network nsg rule show -g "$GRUPO" --nsg-name "$NSG" -n "$REGLA" --query "sourceAddressPrefix" -o tsv
    az network nsg rule show -g "$GRUPO" --nsg-name "$NSG" -n "$REGLA" --query "sourceAddressPrefixes[]" -o tsv
  } | grep -E '^[0-9]' | sort -u
}

es_ip() { [[ "$1" =~ ^[0-9]{1,3}(\.[0-9]{1,3}){3}$ ]]; }

guardar() {   # recibe la lista final de prefijos
  [ "$#" -gt 0 ] || { echo "La lista quedaria vacia; no la cambio."; exit 1; }
  az network nsg rule update -g "$GRUPO" --nsg-name "$NSG" -n "$REGLA" \
    --source-address-prefixes "$@" -o none
  # Practica 2: Grafana (3000) se ve desde las mismas IPs que el SSH
  if az network nsg rule show -g "$GRUPO" --nsg-name "$NSG" -n permitir-grafana-mis-ips -o none 2>/dev/null; then
    az network nsg rule update -g "$GRUPO" --nsg-name "$NSG" -n permitir-grafana-mis-ips \
      --source-address-prefixes "$@" -o none
    echo "(Grafana actualizado con la misma lista)"
  fi
}

LISTA="$(actuales)"

case "${1:-}" in
  --ver)
    echo "IPs que hoy pueden entrar por SSH:"; echo "$LISTA" | sed 's/^/  /'; exit 0 ;;
  --quitar)
    IP="${2:?Falta la IP a quitar}"; IP="${IP%/32}"
    NUEVA="$(echo "$LISTA" | grep -vx "$IP/32" || true)"
    [ "$NUEVA" != "$LISTA" ] || { echo "$IP no estaba en la lista."; exit 0; }
    # shellcheck disable=SC2086
    guardar $NUEVA
    echo "Quitada $IP. Quedan:"; echo "$NUEVA" | sed 's/^/  /'; exit 0 ;;
  "")
    IP="$(curl -s --max-time 10 https://api.ipify.org || true)" ;;
  *)
    IP="$1" ;;
esac

IP="${IP%/32}"
es_ip "$IP" || { echo "No pude saber tu IP publica (salio: '$IP'). Pasala a mano: bash agregar_mi_ip.sh 1.2.3.4"; exit 1; }

if echo "$LISTA" | grep -qx "$IP/32"; then
  echo "Tu IP $IP ya estaba autorizada. No cambio nada."
else
  # shellcheck disable=SC2086
  guardar $LISTA "$IP/32"
  echo "Agregada $IP."
fi
echo "IPs que hoy pueden entrar por SSH:"; actuales | sed 's/^/  /'
