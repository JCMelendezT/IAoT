output "dashboard_url" {
  description = "Direccion publica del dashboard (S3 web estatica, solo HTTP)"
  value       = "http://${aws_s3_bucket_website_configuration.web.website_endpoint}"
}

output "api_url" {
  value = aws_apigatewayv2_api.api.api_endpoint
}

output "iot_endpoint" {
  description = "Servidor MQTT de AWS IoT Core (puerto 8883)"
  value       = data.aws_iot_endpoint.datos.endpoint_address
}

output "token_api" {
  description = "Clave que te pide el dashboard"
  value       = local.token
  sensitive   = true
}

output "tabla_lecturas" {
  value = aws_dynamodb_table.lecturas.name
}

output "ver_token" {
  value = "terraform output -raw token_api"
}
