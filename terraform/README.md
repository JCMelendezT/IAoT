# Terraform

La infraestructura de las prácticas como código. Cada nube tiene su carpeta y el flujo es el mismo: `init`, `plan`, `apply` y `destroy`.

| Carpeta | Qué crea | Estado |
| --- | --- | --- |
| `azure/` | Grupo `rg-iaot-p1`, red con subredes `app`, `db` e `influx`, NSG, IP estática, las dos VM y PostgreSQL privado (Prácticas 1 y 2) | Escrito y revisado, pero nunca aplicado: esas prácticas las desplegué con los scripts de la CLI |
| `aws/` | IoT Core, DynamoDB, Lambda, API Gateway y el bucket del tablero (Práctica 3) | Aplicado y probado en el Learner Lab |

## Azure

```bash
az login
cd terraform/azure
cp terraform.tfvars.example terraform.tfvars    # mis IPs con /32 y la clave de la base
terraform init
terraform apply
bash ../../practica-1/nube/desplegar_app.sh
bash ../../practica-2/nube/desplegar_p2.sh
```

Terraform deja los mismos `infra.env` que generaban los scripts, así que los de despliegue funcionan igual. Con `habilitar_p2 = false` crea solo lo de la Práctica 1. No hay que mezclar: si la infraestructura ya existe creada con los scripts, primero se borra con `practica-1/nube/borrar_infra.sh`.

## AWS

```bash
cd terraform/aws
terraform init
terraform apply
terraform output -raw token_api
```

Las credenciales del Learner Lab van en `~/.aws/credentials` y cambian en cada sesión. El laboratorio no deja crear roles, por eso se usa `LabRole`; en una cuenta normal habría que agregar un `aws_iam_role`. El bucket se crea con la CLI (`terraform_data.bucket_web`) porque `aws_s3_bucket` falla allí. Además de la infraestructura, `apply` escribe el `certs.h` de `practica-3/esp32/devkit_aws` y de `firmware/devkit_centinela`.

## Estado

`terraform.tfstate` queda en cada carpeta, fuera de git, porque guarda claves y certificados. No hay que borrarlo mientras la infraestructura exista: `destroy` lo necesita. Si el Learner Lab se reinicia y los recursos desaparecen, se borran los `terraform.tfstate*` y se vuelve a hacer `apply`. `.terraform.lock.hcl` sí va en git porque fija las versiones de los proveedores.
