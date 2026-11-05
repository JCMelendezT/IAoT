variable "subscription_id" {
  description = "ID de la suscripcion Azure for Students (az account show --query id -o tsv)"
  type        = string
  default     = "1ba330a0-5897-4ef6-8fd1-d304b0e766d1"
}

variable "region" {
  description = "Esta suscripcion solo permite chilecentral y mexicocentral"
  type        = string
  default     = "chilecentral"
}

variable "grupo" {
  description = "Nombre del grupo de recursos (todo vive adentro; destroy lo borra de una)"
  type        = string
  default     = "rg-iaot-p1"
}

variable "usuario_vm" {
  type    = string
  default = "juan"
}

variable "llave_ssh_publica" {
  description = "Ruta de tu llave publica SSH"
  type        = string
  default     = "~/.ssh/id_rsa.pub"
}

variable "mis_ips" {
  description = "IPs (en formato CIDR, por ejemplo 190.1.2.3/32) que pueden entrar por SSH y ver Grafana"
  type        = list(string)
}

variable "clave_bd" {
  description = "Clave del administrador de PostgreSQL (min. 8, mayusculas, minusculas y numeros)"
  type        = string
  sensitive   = true
}

variable "admin_bd" {
  type    = string
  default = "iaotadmin"
}

variable "talla_vm" {
  description = "B1s no esta permitida en esta suscripcion"
  type        = string
  default     = "Standard_B2ats_v2"
}

variable "habilitar_p2" {
  description = "true = tambien crea lo de la Practica 2 (subred privada y vm-influx, puertos 1883 y 3000)"
  type        = bool
  default     = true
}
