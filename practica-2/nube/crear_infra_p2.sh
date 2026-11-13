#!/usr/bin/env bash
# Practica 2: agrega a la red de la Practica 1 lo que falta para MQTT + InfluxDB + Grafana.
#
# Reutiliza (NO toca): grupo rg-iaot-p1, red vnet-iaot, VM vm-iaot (subred publica app)
# con su IP estatica, y todo lo de PostgreSQL.
#
# Crea:
#   - Subred PRIVADA influx 10.0.3.0/24, con su NSG nsg-influx:
#       8086 (InfluxDB) y 22 (SSH) solo desde la subred app; el resto de la red, negado.
#   - VM vm-influx (Ubuntu 24.04, B2ats_v2) SIN IP publica, IP fija 10.0.3.4.
#   - En nsg-app: 1883 (MQTT) desde internet y 3000 (Grafana) solo desde tus IPs.
#
# COSTO: vm-influx ~US$ 10/mes encendida (+ disco ~US$ 2-6/mes). Se apaga con pausar_p2.sh.
#
# Uso (Ubuntu/WSL, con "az login" hecho), desde esta carpeta:
#   bash crear_infra_p2.sh
set -euo pipefail
cd "$(dirname "$0")"

GRUPO="rg-iaot-p1"
REGION="${REGION:-chilecentral}"
VNET="vnet-iaot"
USUARIO_VM="juan"
IP_INFLUX="10.0.3.4"
TALLA="Standard_B2ats_v2"

echo "== Revisando la sesion de Azure =="
az account show --query "{Suscripcion:name, Usuario:user.name}" -o table
az group show -n "$GRUPO" -o none 2>/dev/null || { echo "No existe $GRUPO. Primero la Practica 1 (practica-1/nube/crear_infra.sh)."; exit 1; }
[ -f "$HOME/.ssh/id_rsa.pub" ] || { echo "No encuentro tu llave SSH ($HOME/.ssh/id_rsa.pub), la misma de la Practica 1."; exit 1; }

echo "== 1/4 Subred privada influx 10.0.3.0/24 =="
if ! az network vnet subnet show -g "$GRUPO" --vnet-name "$VNET" -n influx -o none 2>/dev/null; then
  az network vnet subnet create -g "$GRUPO" --vnet-name "$VNET" -n influx \
    --address-prefixes 10.0.3.0/24 -o none
else
  echo "   (ya existe, sigo)"
fi

echo "== 2/4 Grupo de seguridad nsg-influx =="
if ! az network nsg show -g "$GRUPO" -n nsg-influx -o none 2>/dev/null; then
  az network nsg create -g "$GRUPO" -l "$REGION" -n nsg-influx -o none
  for i in $(seq 1 12); do az network nsg show -g "$GRUPO" -n nsg-influx -o none 2>/dev/null && break; sleep 5; done
fi
az network nsg rule create -g "$GRUPO" --nsg-name nsg-influx -n permitir-8086-desde-app \
  --priority 100 --direction Inbound --access Allow --protocol Tcp \
  --source-address-prefixes 10.0.1.0/24 --destination-port-ranges 8086 -o none
az network nsg rule create -g "$GRUPO" --nsg-name nsg-influx -n permitir-ssh-desde-app \
  --priority 110 --direction Inbound --access Allow --protocol Tcp \
  --source-address-prefixes 10.0.1.0/24 --destination-port-ranges 22 -o none
az network nsg rule create -g "$GRUPO" --nsg-name nsg-influx -n negar-resto-vnet \
  --priority 200 --direction Inbound --access Deny --protocol '*' \
  --source-address-prefixes VirtualNetwork --destination-port-ranges '*' -o none
az network vnet subnet update -g "$GRUPO" --vnet-name "$VNET" -n influx \
  --network-security-group nsg-influx -o none

echo "== 3/4 Reglas nuevas en nsg-app (MQTT y Grafana) =="
az network nsg rule create -g "$GRUPO" --nsg-name nsg-app -n permitir-mqtt-1883 \
  --priority 120 --direction Inbound --access Allow --protocol Tcp \
  --source-address-prefixes Internet --destination-port-ranges 1883 -o none
# Grafana: mismas IPs que ya pueden entrar por SSH (las mantiene agregar_mi_ip.sh)
MIS_IPS=$( { az network nsg rule show -g "$GRUPO" --nsg-name nsg-app -n permitir-ssh-mi-ip --query "sourceAddressPrefix" -o tsv
             az network nsg rule show -g "$GRUPO" --nsg-name nsg-app -n permitir-ssh-mi-ip --query "sourceAddressPrefixes[]" -o tsv; } \
           | grep -E '^[0-9]' | sort -u | tr '\n' ' ')
echo "   Grafana visible desde: $MIS_IPS"
# shellcheck disable=SC2086
az network nsg rule create -g "$GRUPO" --nsg-name nsg-app -n permitir-grafana-mis-ips \
  --priority 130 --direction Inbound --access Allow --protocol Tcp \
  --source-address-prefixes $MIS_IPS --destination-port-ranges 3000 -o none

echo "== 4/4 VM vm-influx (sin IP publica) =="
if az vm show -g "$GRUPO" -n vm-influx -o none 2>/dev/null; then
  echo "   (ya existe, sigo)"
else
  az vm create -g "$GRUPO" -l "$REGION" -n vm-influx \
    --image Canonical:ubuntu-24_04-lts:server:latest --size "$TALLA" \
    --vnet-name "$VNET" --subnet influx --private-ip-address "$IP_INFLUX" \
    --public-ip-address "" --nsg "" \
    --admin-username "$USUARIO_VM" --ssh-key-values "$HOME/.ssh/id_rsa.pub" -o none
fi

cat > infra_p2.env <<EOF
IP_INFLUX=$IP_INFLUX
EOF

echo
echo "LISTO."
echo "  InfluxDB ira en vm-influx ($IP_INFLUX), sin IP publica."
echo "  Broker MQTT: $IP_PUBLICA:1883   Grafana: http://$IP_PUBLICA:3000 (solo desde tus IPs)"
echo "Siguiente paso: bash desplegar_p2.sh"
