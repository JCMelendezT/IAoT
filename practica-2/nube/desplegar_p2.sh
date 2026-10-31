#!/usr/bin/env bash
# Practica 2: instala y configura todo en las dos VMs.
#
#   vm-iaot  (publica, 57.156.64.36): broker Mosquitto (1883, con usuario y clave),
#            puente MQTT -> InfluxDB + CSV (servicio puente-mqtt) y Grafana (3000).
#   vm-influx (privada, 10.0.3.4):    InfluxDB 2 (8086), solo alcanzable desde vm-iaot.
#
# Requisitos: haber corrido crear_infra_p2.sh. Se puede correr varias veces.
# Uso (Ubuntu/WSL, desde esta carpeta):   bash desplegar_p2.sh
#
# Las claves se generan solas la primera vez y quedan en secretos_p2.env
# (solo en tu PC; *.env esta en .gitignore). Al final se imprimen las que necesitas.
set -euo pipefail
cd "$(dirname "$0")"
P2="$(cd .. && pwd)"

source ../../practica-1/nube/infra.env      # IP_PUBLICA, USUARIO_VM
source infra_p2.env                         # IP_INFLUX

# ------------------------------------------------------------------ claves
if [ ! -f secretos_p2.env ]; then
  echo "== Generando claves nuevas (secretos_p2.env) =="
  gen() { openssl rand -hex "$1"; }
  echo "Para recibir las alertas en Telegram necesitas un bot (ver README, paso 'Telegram')."
  read -r -p "Token del bot de Telegram (Enter para dejarlo para despues): " TG_TOKEN
  read -r -p "Chat ID de Telegram (Enter para dejarlo para despues): " TG_CHAT
  umask 077
  cat > secretos_p2.env <<EOF
MQTT_CLAVE_ESP32=$(gen 8)
MQTT_CLAVE_PUENTE=$(gen 12)
MQTT_CLAVE_JUAN=$(gen 8)
INFLUX_CLAVE_ADMIN=$(gen 12)
INFLUX_TOKEN=$(gen 32)
GRAFANA_CLAVE_ADMIN=$(gen 8)
TELEGRAM_TOKEN=$TG_TOKEN
TELEGRAM_CHAT_ID=$TG_CHAT
EOF
fi
source secretos_p2.env

VM="$USUARIO_VM@$IP_PUBLICA"
SSHO=(-o StrictHostKeyChecking=accept-new -o ConnectTimeout=15)
en_vm()     { ssh "${SSHO[@]}" "$VM" "$@"; }
en_influx() { ssh "${SSHO[@]}" -J "$VM" "$USUARIO_VM@$IP_INFLUX" "$@"; }

echo "== 0/6 Probando acceso a las dos VMs =="
en_vm "echo '   vm-iaot OK'"
en_influx "echo '   vm-influx OK (por salto desde vm-iaot)'"

# ------------------------------------------------------------------ 1. memoria extra en vm-iaot
echo "== 1/6 Swap de 1 GB en vm-iaot (Grafana + broker + puente en 1 GB de RAM) =="
en_vm 'if ! swapon --show | grep -q swapfile; then
  sudo fallocate -l 1G /swapfile && sudo chmod 600 /swapfile && sudo mkswap /swapfile >/dev/null && sudo swapon /swapfile
  grep -q "/swapfile" /etc/fstab || echo "/swapfile none swap sw 0 0" | sudo tee -a /etc/fstab >/dev/null
  echo "   swap creada"; else echo "   (ya hay swap)"; fi
# El servidor HTTP de la Practica 1 depende de PostgreSQL; en la Practica 2 no se usa.
# Se apaga para liberar memoria (vuelve con: sudo systemctl enable --now servidor-detector).
if systemctl is-enabled --quiet servidor-detector 2>/dev/null; then
  sudo systemctl disable --now servidor-detector >/dev/null 2>&1 && echo "   servidor de la Practica 1 apagado (libera memoria)"
fi'

# ------------------------------------------------------------------ 2. broker Mosquitto
echo "== 2/6 Broker Mosquitto en vm-iaot =="
scp "${SSHO[@]}" "$P2/broker/mosquitto.conf" "$P2/broker/acl" "$VM:/tmp/"
en_vm "set -e
  sudo DEBIAN_FRONTEND=noninteractive apt-get -o DPkg::Lock::Timeout=180 update -q >/dev/null
  sudo DEBIAN_FRONTEND=noninteractive apt-get -o DPkg::Lock::Timeout=180 install -y -q mosquitto mosquitto-clients >/dev/null
  sudo install -m 644 /tmp/mosquitto.conf /etc/mosquitto/conf.d/practica2.conf
  sudo install -m 640 -o root -g mosquitto /tmp/acl /etc/mosquitto/acl
  sudo rm -f /etc/mosquitto/passwd
  sudo touch /etc/mosquitto/passwd
  sudo mosquitto_passwd -b /etc/mosquitto/passwd esp32  '$MQTT_CLAVE_ESP32'
  sudo mosquitto_passwd -b /etc/mosquitto/passwd puente '$MQTT_CLAVE_PUENTE'
  sudo mosquitto_passwd -b /etc/mosquitto/passwd juan   '$MQTT_CLAVE_JUAN'
  sudo chown root:mosquitto /etc/mosquitto/passwd && sudo chmod 640 /etc/mosquitto/passwd
  sudo systemctl enable mosquitto >/dev/null 2>&1
  sudo systemctl restart mosquitto
  sleep 2
  systemctl is-active mosquitto | sed 's/^/   mosquitto: /'"

# ------------------------------------------------------------------ 3. InfluxDB en la VM privada
echo "== 3/6 InfluxDB 2 en vm-influx (sin internet: el paquete se baja en vm-iaot y se pasa) =="
if en_influx "systemctl is-active --quiet influxdb"; then
  echo "   (InfluxDB ya esta instalado y corriendo)"
else
  en_vm "set -e
    if [ ! -f /etc/apt/sources.list.d/influxdata.list ]; then
      sudo mkdir -p /etc/apt/keyrings
      curl -fsSL https://repos.influxdata.com/influxdata-archive.key | gpg --dearmor | sudo tee /etc/apt/keyrings/influxdata.gpg >/dev/null
      echo 'deb [signed-by=/etc/apt/keyrings/influxdata.gpg] https://repos.influxdata.com/debian stable main' | sudo tee /etc/apt/sources.list.d/influxdata.list >/dev/null
      sudo apt-get -o DPkg::Lock::Timeout=180 update -q >/dev/null
    fi
    rm -rf ~/paquetes-influx && mkdir ~/paquetes-influx && cd ~/paquetes-influx
    apt-get download influxdb2 influxdb2-cli >/dev/null
    ls -1 *.deb | sed 's/^/   bajado: /'"
  mkdir -p /tmp/paquetes-influx && rm -f /tmp/paquetes-influx/*.deb
  scp "${SSHO[@]}" "$VM:paquetes-influx/*.deb" /tmp/paquetes-influx/
  scp "${SSHO[@]}" -J "$VM" /tmp/paquetes-influx/*.deb "$USUARIO_VM@$IP_INFLUX:/tmp/"
  en_influx "set -e; sudo dpkg -i /tmp/influxdb2*.deb >/dev/null; sudo systemctl enable --now influxdb >/dev/null 2>&1; echo '   InfluxDB instalado'"
fi
echo "   Esperando a que InfluxDB responda..."
en_influx 'for i in $(seq 1 30); do curl -sf http://localhost:8086/health >/dev/null && break; sleep 2; done; curl -s http://localhost:8086/health | head -c 120; echo'
# Configuracion inicial (usuario admin, organizacion uao, bucket detector, token fijo). Si ya existe, no hace nada.
en_influx "curl -s -o /dev/null -w '   setup de InfluxDB: HTTP %{http_code} (201 = creado, 422 = ya estaba)\n' \
  -X POST http://localhost:8086/api/v2/setup -H 'Content-Type: application/json' \
  -d '{\"username\":\"admin\",\"password\":\"$INFLUX_CLAVE_ADMIN\",\"org\":\"uao\",\"bucket\":\"detector\",\"token\":\"$INFLUX_TOKEN\",\"retentionPeriodSeconds\":0}'"

# ------------------------------------------------------------------ 4. puente MQTT -> Influx + CSV
echo "== 4/6 Puente MQTT -> InfluxDB + CSV en vm-iaot =="
en_vm "mkdir -p ~/practica2/puente ~/practica2/datos"
scp "${SSHO[@]}" "$P2/puente/puente_mqtt.py" "$P2/puente/requirements.txt" "$P2/puente/puente-mqtt.service" "$VM:practica2/puente/"
en_vm "umask 077; cat > ~/practica2/puente/.env" <<EOF
MQTT_HOST=localhost
MQTT_PUERTO=1883
MQTT_USUARIO=puente
MQTT_CLAVE=$MQTT_CLAVE_PUENTE
MQTT_TOPIC=detector1/#
INFLUX_URL=http://$IP_INFLUX:8086
INFLUX_ORG=uao
INFLUX_BUCKET=detector
INFLUX_TOKEN=$INFLUX_TOKEN
DATOS_DIR=/home/$USUARIO_VM/practica2/datos
UMBRAL_BANDA=2.5
EOF
en_vm "set -e
  [ -x ~/venv-p2/bin/python ] || python3 -m venv ~/venv-p2
  ~/venv-p2/bin/pip install -q -r ~/practica2/puente/requirements.txt
  sed 's/__USUARIO__/$USUARIO_VM/g' ~/practica2/puente/puente-mqtt.service | sudo tee /etc/systemd/system/puente-mqtt.service >/dev/null
  sudo systemctl daemon-reload && sudo systemctl enable puente-mqtt >/dev/null 2>&1 && sudo systemctl restart puente-mqtt
  sleep 3; systemctl is-active puente-mqtt | sed 's/^/   puente-mqtt: /'
  journalctl -u puente-mqtt -n 3 --no-pager -o cat | sed 's/^/   | /'"

# ------------------------------------------------------------------ 5. Grafana
echo "== 5/6 Grafana en vm-iaot =="
scp "${SSHO[@]}" "$P2/grafana/datasource.yaml" "$P2/grafana/dashboards.yaml" "$P2/grafana/detector.json" \
    "$P2/grafana/reglas.yaml" "$P2/grafana/notificaciones.yaml" "$VM:/tmp/"
CON_TELEGRAM=0; [ -n "${TELEGRAM_TOKEN:-}" ] && [ -n "${TELEGRAM_CHAT_ID:-}" ] && CON_TELEGRAM=1
en_vm "set -e
  if ! dpkg -s grafana >/dev/null 2>&1; then
    sudo mkdir -p /etc/apt/keyrings
    curl -fsSL https://apt.grafana.com/gpg.key | gpg --dearmor | sudo tee /etc/apt/keyrings/grafana.gpg >/dev/null
    echo 'deb [signed-by=/etc/apt/keyrings/grafana.gpg] https://apt.grafana.com stable main' | sudo tee /etc/apt/sources.list.d/grafana.list >/dev/null
    sudo apt-get -o DPkg::Lock::Timeout=180 update -q >/dev/null
    sudo DEBIAN_FRONTEND=noninteractive apt-get -o DPkg::Lock::Timeout=180 install -y -q grafana >/dev/null
  fi
  # Variables para los archivos de provisioning (entre marcas, para poder repetir)
  sudo sed -i '/# >>> practica2/,/# <<< practica2/d' /etc/default/grafana-server
  sudo tee -a /etc/default/grafana-server >/dev/null <<EOT
# >>> practica2
INFLUX_URL=http://$IP_INFLUX:8086
INFLUX_TOKEN=$INFLUX_TOKEN
TELEGRAM_TOKEN=${TELEGRAM_TOKEN:-}
TELEGRAM_CHAT_ID=${TELEGRAM_CHAT_ID:-}
GF_SECURITY_ADMIN_PASSWORD=$GRAFANA_CLAVE_ADMIN
GF_USERS_ALLOW_SIGN_UP=false
GF_ANALYTICS_REPORTING_ENABLED=false
# <<< practica2
EOT
  sudo chmod 640 /etc/default/grafana-server && sudo chown root:grafana /etc/default/grafana-server
  P=/etc/grafana/provisioning
  sudo install -m 640 -o root -g grafana /tmp/datasource.yaml \$P/datasources/detector.yaml
  sudo install -m 640 -o root -g grafana /tmp/dashboards.yaml \$P/dashboards/detector.yaml
  sudo mkdir -p /var/lib/grafana/dashboards/detector
  sudo install -m 644 -o grafana -g grafana /tmp/detector.json /var/lib/grafana/dashboards/detector/detector.json
  sudo install -m 640 -o root -g grafana /tmp/reglas.yaml \$P/alerting/reglas.yaml
  if [ $CON_TELEGRAM = 1 ]; then
    sudo install -m 640 -o root -g grafana /tmp/notificaciones.yaml \$P/alerting/notificaciones.yaml
  else
    sudo rm -f \$P/alerting/notificaciones.yaml
  fi
  sudo systemctl daemon-reload && sudo systemctl enable grafana-server >/dev/null 2>&1
  sudo systemctl restart grafana-server
  for i in \$(seq 1 40); do curl -sf http://localhost:3000/api/health >/dev/null && break; sleep 3; done
  if ! curl -sf http://localhost:3000/api/health >/dev/null; then
    echo '   !! Grafana no arranco con las alertas. Las quito y lo dejo arrancar; mira el error:'
    sudo journalctl -u grafana-server -n 15 --no-pager -o cat | grep -iE 'error|fail' | tail -5 | sed 's/^/   | /'
    sudo mkdir -p /etc/grafana/alerting-desactivado && sudo mv \$P/alerting/*.yaml /etc/grafana/alerting-desactivado/ 2>/dev/null || true
    sudo systemctl restart grafana-server; sleep 15
  fi
  # La clave de admin solo se toma la primera vez; esto la fija siempre
  sudo grafana cli --homepath /usr/share/grafana --config /etc/grafana/grafana.ini admin reset-admin-password '$GRAFANA_CLAVE_ADMIN' >/dev/null 2>&1 \
    || sudo grafana-cli admin reset-admin-password '$GRAFANA_CLAVE_ADMIN' >/dev/null 2>&1 || true
  sudo chown -R grafana:grafana /var/lib/grafana
  curl -s http://localhost:3000/api/health | tr -d '\n' | sed 's/^/   grafana: /'; echo"

# ------------------------------------------------------------------ 6. prueba de punta a punta
echo "== 6/6 Prueba: publico un latido de prueba y lo busco en InfluxDB =="
en_vm "mosquitto_pub -h localhost -u juan -P '$MQTT_CLAVE_JUAN' -t detector1/prueba/latido -m '{\"contador\":1,\"ms\":1,\"rssi\":-50}' -q 1 && echo '   publicado en detector1/prueba/latido'"
sleep 3
en_vm "curl -s -X POST 'http://$IP_INFLUX:8086/api/v2/query?org=uao' -H 'Authorization: Token $INFLUX_TOKEN' \
  -H 'Content-Type: application/vnd.flux' -H 'Accept: application/csv' \
  -d 'from(bucket:\"detector\") |> range(start:-10m) |> filter(fn:(r)=> r.nodo==\"prueba\") |> count()' \
  | grep -q ',contador,' && echo '   OK: el dato llego a InfluxDB (broker -> puente -> InfluxDB)' \
  || echo '   !! No encontre el dato en InfluxDB: mira  journalctl -u puente-mqtt -n 20'"
en_vm "tail -n 1 ~/practica2/datos/latidos.csv 2>/dev/null | sed 's/^/   CSV: /'"

cat <<EOF

==================================================================
 LISTO. Datos para lo que sigue (estan tambien en nube/secretos_p2.env):

  Broker MQTT ......... $IP_PUBLICA  puerto 1883
  Placa (secrets.h) ... usuario esp32   clave $MQTT_CLAVE_ESP32
  MQTT Explorer ....... usuario juan    clave $MQTT_CLAVE_JUAN
  Grafana ............. http://$IP_PUBLICA:3000   usuario admin   clave $GRAFANA_CLAVE_ADMIN
  Telegram ............ $( [ $CON_TELEGRAM = 1 ] && echo "configurado" || echo "SIN configurar (ver README)" )

 No compartas esta pantalla: tiene claves.
==================================================================
EOF
