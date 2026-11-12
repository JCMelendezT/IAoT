# Practica 3 - arquitectura serverless en AWS
#
#   ESP32 --MQTT/TLS 8883--> AWS IoT Core --regla--> DynamoDB (lecturas)
#   Dashboard (S3 web estatica) --HTTPS--> API Gateway --> Lambda --> DynamoDB
#                                                                 \--> IoT Core (comando al actuador)
#
# En Learner Lab no se crean roles de IAM: Lambda y las reglas usan el rol existente "LabRole".

data "aws_caller_identity" "yo" {}

data "aws_iam_role" "lab" {
  name = var.nombre_rol
}

data "aws_iot_endpoint" "datos" {
  endpoint_type = "iot:Data-ATS"
}

locals {
  cuenta = data.aws_caller_identity.yo.account_id
  rol    = data.aws_iam_role.lab.arn
  token  = var.token_api != "" ? var.token_api : random_password.token.result
  raiz   = "detector1"
}

resource "random_password" "token" {
  length  = 20
  special = false
}

# ------------------------------------------------------------------ DynamoDB
resource "aws_dynamodb_table" "lecturas" {
  name         = "${var.proyecto}-lecturas"
  billing_mode = "PAY_PER_REQUEST"
  hash_key     = "nodo"
  range_key    = "ts"

  attribute {
    name = "nodo"
    type = "S"
  }
  attribute {
    name = "ts"
    type = "N"
  }
}

# Una sola fila por actuador: siempre se sobreescribe con el ultimo estado.
resource "aws_dynamodb_table" "estado" {
  name         = "${var.proyecto}-estado"
  billing_mode = "PAY_PER_REQUEST"
  hash_key     = "nodo"

  attribute {
    name = "nodo"
    type = "S"
  }
}

# ------------------------------------------------------------------ AWS IoT Core: cosa, certificado y politica
resource "aws_iot_thing" "devkit" {
  name = var.nombre_cosa
}

resource "aws_iot_certificate" "devkit" {
  active = true
}

resource "aws_iot_policy" "devkit" {
  name = "${var.proyecto}-politica-devkit"
  policy = jsonencode({
    Version = "2012-10-17"
    Statement = [
      {
        Effect   = "Allow"
        Action   = "iot:Connect"
        Resource = "arn:aws:iot:${var.region}:${local.cuenta}:client/${var.nombre_cosa}*"
      },
      {
        Effect   = "Allow"
        Action   = "iot:Publish"
        Resource = "arn:aws:iot:${var.region}:${local.cuenta}:topic/${local.raiz}/*"
      },
      {
        Effect   = "Allow"
        Action   = "iot:Subscribe"
        Resource = "arn:aws:iot:${var.region}:${local.cuenta}:topicfilter/${local.raiz}/actuador/cmd"
      },
      {
        Effect   = "Allow"
        Action   = "iot:Receive"
        Resource = "arn:aws:iot:${var.region}:${local.cuenta}:topic/${local.raiz}/actuador/cmd"
      }
    ]
  })
}

resource "aws_iot_policy_attachment" "devkit" {
  policy = aws_iot_policy.devkit.name
  target = aws_iot_certificate.devkit.arn
}

resource "aws_iot_thing_principal_attachment" "devkit" {
  thing     = aws_iot_thing.devkit.name
  principal = aws_iot_certificate.devkit.arn
}

# ------------------------------------------------------------------ Reglas: MQTT -> DynamoDB
# El nombre del nodo sale del topic: detector1/<nodo>/lectura  -> topic(2)
resource "aws_iot_topic_rule" "lecturas" {
  name        = replace("${var.proyecto}_lecturas", "-", "_")
  enabled     = true
  sql         = "SELECT *, topic(2) AS nodo, topic(3) AS tema, timestamp() AS ts FROM '${local.raiz}/+/lectura'"
  sql_version = "2016-03-23"

  dynamodbv2 {
    role_arn = local.rol
    put_item {
      table_name = aws_dynamodb_table.lecturas.name
    }
  }
}

resource "aws_iot_topic_rule" "eventos" {
  name        = replace("${var.proyecto}_eventos", "-", "_")
  enabled     = true
  sql         = "SELECT *, topic(2) AS nodo, topic(3) AS tema, timestamp() AS ts FROM '${local.raiz}/+/evento'"
  sql_version = "2016-03-23"

  dynamodbv2 {
    role_arn = local.rol
    put_item {
      table_name = aws_dynamodb_table.lecturas.name
    }
  }
}

resource "aws_iot_topic_rule" "estado" {
  name        = replace("${var.proyecto}_estado_actuador", "-", "_")
  enabled     = true
  sql         = "SELECT *, 'actuador' AS nodo, timestamp() AS ts FROM '${local.raiz}/actuador/estado'"
  sql_version = "2016-03-23"

  dynamodbv2 {
    role_arn = local.rol
    put_item {
      table_name = aws_dynamodb_table.estado.name
    }
  }
}

# ------------------------------------------------------------------ Lambda
data "archive_file" "api" {
  type        = "zip"
  source_file = "${path.module}/lambda/api.py"
  output_path = "${path.module}/.build/api.zip"
}

resource "aws_lambda_function" "api" {
  function_name    = "${var.proyecto}-api"
  role             = local.rol
  runtime          = "python3.12"
  handler          = "api.handler"
  filename         = data.archive_file.api.output_path
  source_code_hash = data.archive_file.api.output_base64sha256
  timeout          = 15
  memory_size      = 256

  environment {
    variables = {
      TABLA_LECTURAS = aws_dynamodb_table.lecturas.name
      TABLA_ESTADO   = aws_dynamodb_table.estado.name
      IOT_ENDPOINT   = data.aws_iot_endpoint.datos.endpoint_address
      NODO           = var.nodo
      RAIZ           = local.raiz
      TOKEN          = local.token
    }
  }
}

# ------------------------------------------------------------------ API Gateway (HTTP API)
resource "aws_apigatewayv2_api" "api" {
  name          = "${var.proyecto}-api"
  protocol_type = "HTTP"

  cors_configuration {
    allow_origins = ["*"]
    allow_methods = ["GET", "POST", "OPTIONS"]
    allow_headers = ["content-type", "x-token"]
    max_age       = 3600
  }
}

resource "aws_apigatewayv2_integration" "lambda" {
  api_id                 = aws_apigatewayv2_api.api.id
  integration_type       = "AWS_PROXY"
  integration_uri        = aws_lambda_function.api.invoke_arn
  payload_format_version = "2.0"
}

resource "aws_apigatewayv2_route" "rutas" {
  for_each  = toset(["GET /lecturas", "GET /csv", "GET /estado", "POST /actuador"])
  api_id    = aws_apigatewayv2_api.api.id
  route_key = each.value
  target    = "integrations/${aws_apigatewayv2_integration.lambda.id}"
}

resource "aws_apigatewayv2_stage" "default" {
  api_id      = aws_apigatewayv2_api.api.id
  name        = "$default"
  auto_deploy = true
}

resource "aws_lambda_permission" "apigw" {
  statement_id  = "AllowAPIGateway"
  action        = "lambda:InvokeFunction"
  function_name = aws_lambda_function.api.function_name
  principal     = "apigateway.amazonaws.com"
  source_arn    = "${aws_apigatewayv2_api.api.execution_arn}/*/*"
}

# ------------------------------------------------------------------ S3: dashboard como web estatica
# El bucket se crea con el AWS CLI y no con "aws_s3_bucket": en Learner Lab una politica del
# laboratorio niega s3:GetBucketObjectLockConfiguration y el recurso aws_s3_bucket falla al
# leerse (error 403 AccessDenied). El resto de recursos de S3 funciona con el nombre del bucket.
locals {
  bucket_web = "${var.proyecto}-web-${local.cuenta}"
  restriccion_region = var.region == "us-east-1" ? "" : "--create-bucket-configuration LocationConstraint=${var.region}"
}

resource "terraform_data" "bucket_web" {
  input = {
    nombre = local.bucket_web
    region = var.region
  }

  provisioner "local-exec" {
    command = "aws s3api head-bucket --bucket ${self.input.nombre} --region ${self.input.region} 2>/dev/null || aws s3api create-bucket --bucket ${self.input.nombre} --region ${self.input.region} ${local.restriccion_region}"
  }

  provisioner "local-exec" {
    when    = destroy
    command = "aws s3 rb s3://${self.input.nombre} --force --region ${self.input.region}"
  }
}

resource "aws_s3_bucket_public_access_block" "web" {
  bucket                  = local.bucket_web
  depends_on              = [terraform_data.bucket_web]
  block_public_acls       = false
  block_public_policy     = false
  ignore_public_acls      = false
  restrict_public_buckets = false
}

resource "aws_s3_bucket_website_configuration" "web" {
  bucket     = local.bucket_web
  depends_on = [terraform_data.bucket_web]
  index_document {
    suffix = "index.html"
  }
}

resource "aws_s3_bucket_policy" "web" {
  bucket = local.bucket_web
  policy = jsonencode({
    Version = "2012-10-17"
    Statement = [{
      Sid       = "LecturaPublica"
      Effect    = "Allow"
      Principal = "*"
      Action    = "s3:GetObject"
      Resource  = "arn:aws:s3:::${local.bucket_web}/*"
    }]
  })
  depends_on = [aws_s3_bucket_public_access_block.web]
}

resource "aws_s3_object" "index" {
  bucket       = local.bucket_web
  key          = "index.html"
  source       = "${path.module}/../../practica-3/dashboard/index.html"
  etag         = filemd5("${path.module}/../../practica-3/dashboard/index.html")
  content_type = "text/html; charset=utf-8"
  depends_on   = [aws_s3_bucket_policy.web]
}

resource "aws_s3_object" "config" {
  bucket       = local.bucket_web
  key          = "config.js"
  content_type = "application/javascript"
  content      = <<-EOT
    window.CONFIG = {
      API_URL: "${aws_apigatewayv2_api.api.api_endpoint}",
      NODO: "${var.nodo}"
    };
  EOT
  depends_on   = [aws_s3_bucket_policy.web]
}

# ------------------------------------------------------------------ certificados y config para el ESP32 y el simulador
data "http" "ca_amazon" {
  url = "https://www.amazontrust.com/repository/AmazonRootCA1.pem"
}

resource "local_sensitive_file" "certs_h" {
  filename        = abspath("${path.module}/../../practica-3/esp32/devkit_aws/certs.h")
  file_permission = "0600"
  content = templatefile("${path.module}/certs.h.tpl", {
    endpoint = data.aws_iot_endpoint.datos.endpoint_address
    cosa     = aws_iot_thing.devkit.name
    ca       = trimspace(data.http.ca_amazon.response_body)
    crt      = trimspace(aws_iot_certificate.devkit.certificate_pem)
    llave    = trimspace(aws_iot_certificate.devkit.private_key)
  })
}

# La misma configuracion para devkit_centinela (pasarela con la app del celular)
resource "local_sensitive_file" "certs_h_centinela" {
  filename        = abspath("${path.module}/../../Lab_ESP32/firmware/devkit_centinela/certs.h")
  file_permission = "0600"
  content         = local_sensitive_file.certs_h.content
}

resource "local_sensitive_file" "ca" {
  filename        = abspath("${path.module}/../../practica-3/certs/AmazonRootCA1.pem")
  file_permission = "0600"
  content         = data.http.ca_amazon.response_body
}

resource "local_sensitive_file" "crt" {
  filename        = abspath("${path.module}/../../practica-3/certs/certificado.pem")
  file_permission = "0600"
  content         = aws_iot_certificate.devkit.certificate_pem
}

resource "local_sensitive_file" "llave" {
  filename        = abspath("${path.module}/../../practica-3/certs/llave-privada.pem")
  file_permission = "0600"
  content         = aws_iot_certificate.devkit.private_key
}

resource "local_sensitive_file" "conexion" {
  filename        = abspath("${path.module}/../../practica-3/certs/conexion.env")
  file_permission = "0600"
  content         = <<-EOT
    IOT_ENDPOINT=${data.aws_iot_endpoint.datos.endpoint_address}
    COSA=${aws_iot_thing.devkit.name}
    API_URL=${aws_apigatewayv2_api.api.api_endpoint}
    TOKEN_API=${local.token}
    DASHBOARD_URL=http://${aws_s3_bucket_website_configuration.web.website_endpoint}
  EOT
}
