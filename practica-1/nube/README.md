# Práctica 1 en Azure

El servidor de la parte 1 en una máquina virtual con IP pública fija, y PostgreSQL en una subred privada. Lo desplegué el 24 de octubre en Chile Central y lo borré después de sustentar. Diagrama en [`../arquitectura.md`](../arquitectura.md#2-en-la-nube-azure).

## Recursos

| Recurso | Nombre | Para qué |
| --- | --- | --- |
| Grupo de recursos | `rg-iaot-p1` | Todo junto, para borrarlo de una vez |
| Red virtual | `vnet-iaot`, 10.0.0.0/16 | Red privada del proyecto |
| Subred pública | `app`, 10.0.1.0/24 | La máquina virtual |
| Subred privada | `db`, 10.0.2.0/24, delegada a PostgreSQL | La base, sin IP pública |
| Grupo de seguridad | `nsg-app` | Entra el 8000 desde internet y el 22 solo desde mis IPs |
| Grupo de seguridad | `nsg-db` | Entra el 5432 solo desde la subred `app` |
| IP pública estática | `ip-iaot` | Equivale a la IP elástica de AWS |
| Máquina virtual | `vm-iaot`, Ubuntu 24.04, B2ats_v2 | FastAPI y CSV como servicio de systemd |
| Base de datos | PostgreSQL Flexible Server 16, B1MS, 32 GB | Las mismas tablas de la parte 1 |
| Zona DNS privada | `iaot-p1.private.postgres.database.azure.com` | La VM encuentra la base por nombre |

La suscripción de estudiante solo dejaba crear recursos en `chilecentral` y `mexicocentral` y no permitía la talla B1s; por eso la VM es B2ats_v2 (unos 10 USD al mes). PostgreSQL B1MS cuesta unos 13 USD al mes y la IP estática cobra aunque la VM esté apagada. Conviene crear antes un presupuesto con alerta en Cost Management.

## Pasos

Desde Ubuntu (WSL), en `practica-1/nube`:

```bash
curl -sL https://aka.ms/InstallAzureCLIDeb | sudo bash   # una vez
az login
bash crear_infra.sh          # 10 a 15 min; pide la clave del administrador de la base
bash desplegar_app.sh        # copia el servidor a la VM y lo deja como servicio
```

Si Azure dice que la región no está permitida: `REGION=mexicocentral bash crear_infra.sh`.

Después el servidor queda en `http://<IP pública>:8000`, y en el devkit basta con poner esa IP en `SERVIDOR_IP` del `secrets.h` de `devkit_pasarela`.

## Comprobaciones

```bash
source infra.env
ssh $USUARIO_VM@$IP_PUBLICA "systemctl --no-pager status servidor-detector | head -3"
ssh -t $USUARIO_VM@$IP_PUBLICA "psql 'host=$HOST_BD dbname=iaot user=$ADMIN_BD sslmode=require' -c 'SELECT count(*) FROM ventanas;'"
ssh $USUARIO_VM@$IP_PUBLICA "wc -l ~/detector/datos/ventanas.csv"
psql "host=$HOST_BD dbname=iaot user=$ADMIN_BD connect_timeout=5"   # desde el PC no debe responder
```

## Scripts

| Script | Qué hace |
| --- | --- |
| `crear_infra.sh` | Crea red, subredes, NSG, IP, VM y PostgreSQL; deja `infra.env` (sin claves, fuera de git) |
| `desplegar_app.sh` | Copia `../servidor`, escribe el `.env` en la VM e instala el servicio |
| `servidor-detector.service` | Servicio de systemd que arranca el servidor y lo reinicia si se cae |
| `agregar_mi_ip.sh` | Agrega la IP pública actual a la regla de SSH sin borrar las demás (`--ver`, `--quitar IP`) |
| `pausar_infra.sh` / `reanudar_infra.sh` | Apagan y encienden la VM y PostgreSQL para no pagar cómputo |
| `borrar_infra.sh` | Borra el grupo `rg-iaot-p1` completo |
