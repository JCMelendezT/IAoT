# Arquitectura — Práctica 1 (detector de drones)

Tres vistas del mismo sistema: cómo funciona hoy en local, cómo queda en Azure y qué guarda la base de datos. GitHub dibuja los diagramas solo (son Mermaid).

En las dos versiones las placas son las mismas: el **ESP32-S3** escucha y decide la alerta en el borde; el **ESP32 devkit** solo hace de pasarela. Lo único que cambia es a dónde apunta `SERVIDOR_IP`.

## 1. En local (funcionando)

📄 **Versión interactiva (Archify):** [arquitectura local](diagramas/arquitectura-local.html)

```mermaid
flowchart LR
    subgraph casa["Casa · WiFi 2,4 GHz · red 192.168.40.0/24"]
        direction LR
        subgraph s3["ESP32-S3 · nodo (sin WiFi propio)"]
            nrf["nRF24L01+ PA+LNA<br/>126 canales 2400–2525 MHz"]
            snf["Sniffer WiFi<br/>modo promiscuo, canales 1–13"]
            regla["Regla local<br/>alerta 'posible dron'"]
            nrf --> regla
            snf --> regla
        end
        dk["ESP32 devkit · pasarela<br/>IP 192.168.40.42"]
        router(["Router WiFi<br/>canal 8"])
        subgraph pc["PC Windows 11 · IP 192.168.40.17"]
            fw{{"Firewall Hyper-V<br/>regla puerto 8000"}}
            subgraph wsl["Ubuntu en WSL2 · red mirrored"]
                api["FastAPI · app.py<br/>puerto 8000"]
                pg[("PostgreSQL<br/>base iaot")]
                csv[["CSV<br/>practica-1/datos"]]
            end
            nav["Navegador<br/>localhost:8000"]
        end
    end
    s3 -- "UART GPIO17 → GPIO16<br/>115200 · líneas V, L, E" --> dk
    dk -- "WiFi" --> router
    router -- "HTTP POST JSON :8000" --> fw
    fw --> api
    api --> pg
    api --> csv
    nav -- "GET / y /api/resumen" --> api
```

| Ruta | Quién la llama | Qué hace |
| --- | --- | --- |
| `POST /api/ventanas` | devkit, cada 2 s | Resumen WiFi + espectro; el servidor recalcula la alerta |
| `POST /api/latido` | devkit (el suyo y el del S3), cada 5 s | Saber que cada placa sigue viva |
| `POST /api/eventos` | devkit, cuando el S3 inicia o termina una alerta | Registrar la decisión tomada en el borde |
| `GET /`, `GET /api/resumen` | navegador | Página en vivo |

## 2. En la nube (Azure)

📄 **Versión interactiva (Archify):** [arquitectura en Azure](diagramas/arquitectura-nube.html)

```mermaid
flowchart LR
    subgraph casa["Casa (cualquier WiFi con internet)"]
        s3["ESP32-S3 · nodo"] -- "UART" --> dk["ESP32 devkit · pasarela"]
    end
    nav["Navegador / celular"]
    yo["Tu PC<br/>(administración)"]

    subgraph azure["Azure for Students · grupo rg-iaot-p1"]
        ip(("IP pública estática<br/>ip-iaot"))
        subgraph vnet["VNet vnet-iaot · 10.0.0.0/16"]
            subgraph app["Subred PÚBLICA app · 10.0.1.0/24 · NSG nsg-app"]
                vm["VM vm-iaot · Ubuntu 24.04 · B2ats_v2<br/>FastAPI + CSV (systemd)"]
            end
            subgraph db["Subred PRIVADA db · 10.0.2.0/24 · NSG nsg-db"]
                pgf[("PostgreSQL Flexible Server 16<br/>B1MS · 32 GB · sin IP pública")]
            end
        end
        dns["Zona DNS privada<br/>iaot-p1.private.postgres.database.azure.com"]
    end

    dk -- "HTTP POST JSON → :8000" --> ip
    nav -- "http://IP:8000" --> ip
    yo -- "SSH :22 solo desde tu IP" --> ip
    ip --> vm
    vm -- "5432 · solo desde 10.0.1.0/24" --> pgf
    vm -. "resuelve el nombre" .-> dns
```

**Reglas de seguridad**

| NSG | Regla | Origen | Puerto | Acción |
| --- | --- | --- | --- | --- |
| nsg-app | permitir-app-8000 | Internet | 8000/TCP | Permitir |
| nsg-app | permitir-ssh-mi-ip | Tu IP pública /32 | 22/TCP | Permitir |
| nsg-db | permitir-5432-desde-app | 10.0.1.0/24 | 5432/TCP | Permitir |
| nsg-db | negar-resto-vnet | VirtualNetwork | todos | Negar |

Scripts para crearlo: [`nube/`](nube/README.md).

## 3. Base de datos

📄 **Versión interactiva (Archify):** [base de datos](diagramas/base-de-datos.html)

Las tres tablas son independientes; se cruzan por `nodo` y por la hora de llegada. El mismo `esquema.sql` sirve en local y en Azure.

```mermaid
erDiagram
    latidos {
        bigserial id PK
        timestamptz recibido_en
        text ip_origen
        text nodo
        bigint contador
        bigint ms
        int rssi "vacío = placa por cable"
    }
    ventanas {
        bigserial id PK
        timestamptz recibido_en
        text ip_origen
        text nodo
        bigint ms
        bigint ventana
        int canal "0 = saltando"
        int tramas
        int aparatos
        int mgmt
        int datos
        real tam_prom
        real rssi_prom
        int rssi_max
        int tramas_debiles
        int aparatos_fuertes
        int tramas_dji
        int rssi_max_dji
        text espectro "126 dígitos 0-8"
        real nivel_banda_dron
        boolean alerta
        text motivo_alerta
    }
    eventos {
        bigserial id PK
        timestamptz recibido_en
        text ip_origen
        text nodo
        bigint ms
        text tipo "alerta_local, fin_alerta"
        text clase
        real probabilidad
        text detalle
    }
```

Cada fila se guarda también, con las mismas columnas, en `datos/latidos.csv`, `datos/ventanas.csv` y `datos/eventos.csv`.
