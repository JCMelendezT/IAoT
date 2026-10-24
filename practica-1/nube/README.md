# Práctica 1, parte 2 — El servidor en Azure

El mismo servidor de la parte 1, ahora en una máquina virtual de Azure con IP pública fija, y la base de datos en una subred privada sin acceso desde internet. Diagrama: [`../arquitectura.md`](../arquitectura.md#2-en-la-nube-azure).

> Estado: **desplegado** (24 oct 2026) en Chile Central: VM `vm-iaot`, PostgreSQL `pg-iaot-a880f1` privado y servicio `servidor-detector` activo. Falta apuntar el devkit a la IP pública y la prueba con las placas.

## Qué se crea

| Recurso | Nombre | Dónde | Por qué |
| --- | --- | --- | --- |
| Grupo de recursos | `rg-iaot-p1` | — | Todo junto, para borrarlo de una vez |
| Red virtual | `vnet-iaot` 10.0.0.0/16 | — | La red privada del proyecto |
| Subred pública | `app` 10.0.1.0/24 | vnet-iaot | Aquí vive la máquina virtual |
| Subred privada | `db` 10.0.2.0/24 (delegada a PostgreSQL) | vnet-iaot | Aquí vive la base, sin IP pública |
| Grupo de seguridad | `nsg-app` | subred app | Entra 8000 desde internet y 22 (SSH) solo desde tu IP |
| Grupo de seguridad | `nsg-db` | subred db | Entra 5432 solo desde 10.0.1.0/24; nada más |
| IP pública estática | `ip-iaot` (Standard) | VM | Equivale a la "IP elástica" de AWS |
| Máquina virtual | `vm-iaot`, Ubuntu 24.04, B2ats_v2 (Chile Central) | subred app | FastAPI + CSV, como servicio systemd |
| Base de datos | PostgreSQL Flexible Server 16, B1MS, 32 GB | subred db | Base `iaot` con las mismas tablas de la parte 1 |
| Zona DNS privada | `iaot-p1.private.postgres.database.azure.com` | vnet-iaot | La VM encuentra la base por nombre, dentro de la red |

## Costo

- Esta suscripción **solo permite desplegar en `chilecentral` y `mexicocentral`** (las demás responden `RequestDisallowedByAzure`) y **no permite la talla B1s**, así que la VM usa `Standard_B2ats_v2`: 2 vCPU y 1 GB por unos **10 USD/mes** en `chilecentral`.
- PostgreSQL B1MS (32 GB) cuesta unos **13 USD/mes**.
- La **IP pública estática cobra aunque la VM esté apagada** (unos pocos dólares al mes).
- Antes de empezar: crea un presupuesto con alerta en *Cost Management → Presupuestos* (por ejemplo 5 USD).
- Al terminar: `bash borrar_infra.sh`.

## Pasos (en Ubuntu/WSL, dentro de `practica-1/nube`)

1. Instalar la CLI de Azure e iniciar sesión (una vez):

   ```bash
   curl -sL https://aka.ms/InstallAzureCLIDeb | sudo bash
   az login
   ```

2. Crear la infraestructura (10 a 15 minutos). Pide una clave para el administrador de la base:

   ```bash
   bash crear_infra.sh
   ```

   Si Azure dice que la región no está permitida: `REGION=mexicocentral bash crear_infra.sh`.

3. Copiar el servidor a la VM y encenderlo:

   ```bash
   bash desplegar_app.sh
   ```

4. Abrir `http://<IP pública>:8000` desde cualquier lugar.
5. En el devkit: poner la IP pública en `SERVIDOR_IP` del `secrets.h` de `devkit_pasarela` y volver a subirlo. Ya no importa en qué WiFi esté: solo necesita internet.

## Verificar (evidencia)

Desde Ubuntu en tu PC, dentro de `practica-1/nube`:

```bash
source infra.env
# 1. El servidor corre en la VM
ssh $USUARIO_VM@$IP_PUBLICA "systemctl --no-pager status servidor-detector | head -3"
# 2. La base responde, pero desde DENTRO de la VM (pide la clave de la base)
ssh -t $USUARIO_VM@$IP_PUBLICA "psql 'host=$HOST_BD dbname=iaot user=$ADMIN_BD sslmode=require' -c 'SELECT count(*) FROM ventanas;'"
# 3. El CSV crece en la VM
ssh $USUARIO_VM@$IP_PUBLICA "wc -l ~/detector/datos/ventanas.csv"
# 4. Desde tu PC la base NO responde: no tiene ruta desde internet
psql "host=$HOST_BD dbname=iaot user=$ADMIN_BD connect_timeout=5"
```

## Archivos

| Archivo | Qué hace |
| --- | --- |
| `crear_infra.sh` | Crea red, subredes, NSG, IP, VM y PostgreSQL; deja `infra.env` (sin claves, no se sube) |
| `desplegar_app.sh` | Copia `../servidor`, escribe el `.env` en la VM, instala y enciende el servicio |
| `servidor-detector.service` | Servicio systemd: arranca el servidor y lo reinicia si se cae |
| `borrar_infra.sh` | Borra todo el grupo `rg-iaot-p1` |
| `pausar_infra.sh` | Apaga VM y PostgreSQL: deja de cobrar el cómputo (~0,5 USD/día en vez de ~1,3) |
| `reanudar_infra.sh` | Vuelve a encenderlos y muestra la URL del servidor |
