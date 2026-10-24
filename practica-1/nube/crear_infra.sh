#!/usr/bin/env bash
# Practica 1, parte 2: crea la infraestructura en Azure con la CLI "az".
#
# Que crea (todo dentro del grupo rg-iaot-p1, para borrarlo de una vez al final):
#   - Red virtual vnet-iaot 10.0.0.0/16 con dos subredes:
#       app 10.0.1.0/24  (PUBLICA)  -> la maquina virtual con el servidor
#       db  10.0.2.0/24  (PRIVADA)  -> PostgreSQL, sin IP publica
#   - NSG nsg-app: abre 8000 a internet y 22 (SSH) solo desde TU IP
#   - NSG nsg-db : abre 5432 solo desde la subred app; nada mas entra
#   - IP publica ESTATICA ip-iaot (equivale a la "IP elastica" de AWS)
#   - Maquina virtual vm-iaot: Ubuntu 24.04, tamano B1s
#   - PostgreSQL Flexible Server (B1MS, 32 GB) con acceso privado + base "iaot"
#
# COSTO: B1s y PostgreSQL B1MS entran en los 12 meses gratis de Azure for Students
# (750 h/mes cada uno). La IP publica estatica COBRA aunque la VM este apagada.
# Al terminar la practica: bash borrar_infra.sh
#
# Uso (en Ubuntu/WSL, con la CLI instalada y "az login" hecho):
#   bash crear_infra.sh
set -euo pipefail

# ------------------------------------------------------------------ ajustes
GRUPO="rg-iaot-p1"
REGION="${REGION:-eastus2}"          # si Azure for Students no la permite, prueba: REGION=centralus bash crear_infra.sh
VNET="vnet-iaot"
USUARIO_VM="juan"
SUFIJO="$(printf '%s' "$(az account show --query id -o tsv)" | md5sum | cut -c1-6)"
SERVIDOR_BD="pg-iaot-${SUFIJO}"      # el nombre debe ser unico en todo Azure
ADMIN_BD="iaotadmin"

echo "== Revisando la sesion de Azure =="
az account show --query "{suscripcion:name, usuario:user.name}" -o table

MI_IP="$(curl -s https://api.ipify.org)"
echo "Tu IP publica (la unica que podra entrar por SSH): $MI_IP"

if [ -z "${CLAVE_BD:-}" ]; then
  read -r -s -p "Clave para el administrador de la base (min. 8, mayusculas, minusculas y numeros): " CLAVE_BD; echo
fi

echo
echo "== 1/7 Grupo de recursos =="
az group create -n "$GRUPO" -l "$REGION" -o none

echo "== 2/7 Red virtual y subredes =="
az network vnet create -g "$GRUPO" -n "$VNET" --address-prefix 10.0.0.0/16 \
  --subnet-name app --subnet-prefixes 10.0.1.0/24 -o none
az network vnet subnet create -g "$GRUPO" --vnet-name "$VNET" -n db \
  --address-prefixes 10.0.2.0/24 \
  --delegations Microsoft.DBforPostgreSQL/flexibleServers -o none

echo "== 3/7 Grupos de seguridad (NSG) =="
az network nsg create -g "$GRUPO" -n nsg-app -o none
az network nsg rule create -g "$GRUPO" --nsg-name nsg-app -n permitir-app-8000 \
  --priority 100 --direction Inbound --access Allow --protocol Tcp \
  --source-address-prefixes Internet --destination-port-ranges 8000 -o none
az network nsg rule create -g "$GRUPO" --nsg-name nsg-app -n permitir-ssh-mi-ip \
  --priority 110 --direction Inbound --access Allow --protocol Tcp \
  --source-address-prefixes "${MI_IP}/32" --destination-port-ranges 22 -o none
az network vnet subnet update -g "$GRUPO" --vnet-name "$VNET" -n app \
  --network-security-group nsg-app -o none

az network nsg create -g "$GRUPO" -n nsg-db -o none
az network nsg rule create -g "$GRUPO" --nsg-name nsg-db -n permitir-5432-desde-app \
  --priority 100 --direction Inbound --access Allow --protocol Tcp \
  --source-address-prefixes 10.0.1.0/24 --destination-port-ranges 5432 -o none
az network nsg rule create -g "$GRUPO" --nsg-name nsg-db -n negar-resto-vnet \
  --priority 200 --direction Inbound --access Deny --protocol '*' \
  --source-address-prefixes VirtualNetwork --destination-port-ranges '*' -o none
az network vnet subnet update -g "$GRUPO" --vnet-name "$VNET" -n db \
  --network-security-group nsg-db -o none

echo "== 4/7 IP publica estatica =="
az network public-ip create -g "$GRUPO" -n ip-iaot --sku Standard \
  --allocation-method Static -o none

echo "== 5/7 Maquina virtual (Ubuntu 24.04, B1s) =="
az vm create -g "$GRUPO" -n vm-iaot \
  --image Canonical:ubuntu-24_04-lts:server:latest --size Standard_B1s \
  --vnet-name "$VNET" --subnet app --public-ip-address ip-iaot --nsg "" \
  --admin-username "$USUARIO_VM" --generate-ssh-keys -o none

echo "== 6/7 PostgreSQL Flexible Server privado (tarda 5-10 minutos) =="
az postgres flexible-server create -g "$GRUPO" -n "$SERVIDOR_BD" -l "$REGION" \
  --tier Burstable --sku-name Standard_B1ms --storage-size 32 --version 16 \
  --admin-user "$ADMIN_BD" --admin-password "$CLAVE_BD" \
  --vnet "$VNET" --subnet db \
  --private-dns-zone "pg-iaot.private.postgres.database.azure.com" \
  --yes -o none
az postgres flexible-server db create -g "$GRUPO" -s "$SERVIDOR_BD" -d iaot -o none

echo "== 7/7 Resumen =="
IP_PUBLICA="$(az network public-ip show -g "$GRUPO" -n ip-iaot --query ipAddress -o tsv)"
HOST_BD="$(az postgres flexible-server show -g "$GRUPO" -n "$SERVIDOR_BD" --query fullyQualifiedDomainName -o tsv)"

# Datos para el siguiente paso (sin la clave). Este archivo no se sube a GitHub.
cat > infra.env <<EOF2
GRUPO=$GRUPO
IP_PUBLICA=$IP_PUBLICA
USUARIO_VM=$USUARIO_VM
HOST_BD=$HOST_BD
ADMIN_BD=$ADMIN_BD
EOF2

echo
echo "LISTO."
echo "  IP publica de la VM : $IP_PUBLICA   (va en SERVIDOR_IP del devkit)"
echo "  Base de datos       : $HOST_BD (solo visible dentro de la red)"
echo "  Entrar a la VM      : ssh $USUARIO_VM@$IP_PUBLICA"
echo "Siguiente paso: bash desplegar_app.sh"
