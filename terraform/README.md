# Terraform — Azure y AWS

Infraestructura como código para las tres prácticas. Cada nube tiene su carpeta (los recursos son distintos), pero el flujo es el mismo: `init` → `plan` → `apply` → `destroy`. Los scripts `desplegar_*.sh` de las Prácticas 1 y 2 no cambian: Terraform les deja los mismos archivos `infra.env` que antes generaban los `crear_infra*.sh`.

| Carpeta | Qué crea | Equivale a |
| --- | --- | --- |
| `azure/` | Grupo `rg-iaot-p1`, red `vnet-iaot` con subredes `app`, `db` e `influx`, NSG, IP estática, `vm-iaot`, `vm-influx`, PostgreSQL privado | `practica-1/nube/crear_infra.sh` + `practica-2/nube/crear_infra_p2.sh` |
| `aws/` | IoT Core, DynamoDB, Lambda, API Gateway, bucket S3 del dashboard | Práctica 3 (no había script antes) |

## Azure (Prácticas 1 y 2) — Ubuntu (WSL)

```bash
az login
cd terraform/azure
cp terraform.tfvars.example terraform.tfvars    # pon tus IPs (con /32) y la clave de la base
nano terraform.tfvars
terraform init
terraform apply
```

Después, igual que antes:

```bash
bash ../../practica-1/nube/desplegar_app.sh
bash ../../practica-2/nube/desplegar_p2.sh
```

- Para agregar una IP nueva: súmala a `mis_ips` en `terraform.tfvars` y corre `terraform apply` (reemplaza a `agregar_mi_ip.sh`, que también sigue sirviendo con las máquinas creadas por scripts).
- Solo Práctica 1: `habilitar_p2 = false` en `terraform.tfvars`.
- Borrar todo: `terraform destroy`.
- **No mezcles**: si ya creaste la infra con los scripts `az`, no corras `terraform apply` encima (daría errores de recursos que ya existen). Bórrala antes con `bash practica-1/nube/borrar_infra.sh`.

## AWS (Práctica 3) — Ubuntu (WSL)

Guía completa en [`../practica-3/README.md`](../practica-3/README.md). Resumen:

```bash
# credenciales de AWS Academy Learner Lab en ~/.aws/credentials (cambian cada sesión)
cd terraform/aws
terraform init
terraform apply
terraform output -raw token_api     # clave del dashboard
```

Learner Lab no deja crear roles de IAM, por eso `aws/` usa el rol `LabRole` que ya existe (`nombre_rol`). Si en una cuenta normal de AWS quieres crear el rol propio, hay que agregar un `aws_iam_role` y cambiar `local.rol`.

## Estado de Terraform

`terraform.tfstate` queda en la carpeta y **no se sube a git** (tiene claves y certificados). No lo borres mientras la infra exista: es lo que le permite a `terraform destroy` saber qué borrar. En Learner Lab, si el laboratorio se reinicia y los recursos desaparecen, borra `terraform.tfstate*` y vuelve a hacer `apply`.

## Notas de Learner Lab

- Se usa el rol `LabRole` (no se pueden crear roles).
- El bucket del dashboard se crea con el AWS CLI (`terraform_data.bucket_web`): con `aws_s3_bucket` el laboratorio responde 403 al leer la configuración de *object lock*.
- `.terraform.lock.hcl` sí se sube a git (fija las versiones de los proveedores).

## Estado de las pruebas

- **`aws/`:** aplicado y verificado en AWS Academy Learner Lab (IoT Core, DynamoDB, Lambda, API Gateway, S3, dashboard y actuador).
- **`azure/`:** escrito y con la sintaxis revisada, pero **no se ha aplicado** contra una suscripción real; en el primer `terraform plan` pueden salir ajustes pequeños.
