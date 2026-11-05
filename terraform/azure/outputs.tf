output "ip_publica" {
  description = "IP de vm-iaot (va en el devkit y en MQTT Explorer)"
  value       = azurerm_public_ip.ip.ip_address
}

output "host_bd" {
  value = azurerm_postgresql_flexible_server.pg.fqdn
}

output "ssh" {
  value = "ssh ${var.usuario_vm}@${azurerm_public_ip.ip.ip_address}"
}

output "siguiente_paso" {
  value = "bash ../../practica-1/nube/desplegar_app.sh   y luego   bash ../../practica-2/nube/desplegar_p2.sh"
}
