variable "region" {
  description = "AWS Academy Learner Lab solo permite us-east-1 y us-west-2"
  type        = string
  default     = "us-east-1"
}

variable "proyecto" {
  description = "Prefijo para nombrar todo"
  type        = string
  default     = "iaot-p3"
}

variable "nombre_cosa" {
  description = "Nombre de la 'cosa' en AWS IoT Core; tambien es el clientId MQTT del devkit"
  type        = string
  default     = "detector-devkit-1"
}

variable "nodo" {
  description = "Nodo cuyas lecturas muestra el dashboard (segundo nivel del topic detector1/<nodo>/lectura)"
  type        = string
  default     = "nodo-s3"
}

variable "nombre_rol" {
  description = "Rol de IAM que usan Lambda y las reglas de IoT. En Learner Lab no se pueden crear roles: se usa LabRole"
  type        = string
  default     = "LabRole"
}

variable "token_api" {
  description = "Clave que pide el dashboard. Si lo dejas vacio, Terraform genera una"
  type        = string
  default     = ""
  sensitive   = true
}
