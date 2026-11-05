# Equivale a practica-1/nube/crear_infra.sh + practica-2/nube/crear_infra_p2.sh
# Red:  vnet-iaot 10.0.0.0/16
#   app    10.0.1.0/24 (publica)  -> vm-iaot (servidor, Mosquitto, puente, Grafana)
#   db     10.0.2.0/24 (privada)  -> PostgreSQL Flexible Server
#   influx 10.0.3.0/24 (privada)  -> vm-influx (Practica 2)

locals {
  sufijo      = substr(md5(var.subscription_id), 0, 6)
  servidor_bd = "pg-iaot-${local.sufijo}"
  zona_dns    = "iaotdb.postgres.database.azure.com"
  ip_influx   = "10.0.3.4"
  p2          = var.habilitar_p2 ? 1 : 0
}

resource "azurerm_resource_group" "rg" {
  name     = var.grupo
  location = var.region
}

# ------------------------------------------------------------------ red
resource "azurerm_virtual_network" "vnet" {
  name                = "vnet-iaot"
  location            = azurerm_resource_group.rg.location
  resource_group_name = azurerm_resource_group.rg.name
  address_space       = ["10.0.0.0/16"]
}

resource "azurerm_subnet" "app" {
  name                 = "app"
  resource_group_name  = azurerm_resource_group.rg.name
  virtual_network_name = azurerm_virtual_network.vnet.name
  address_prefixes     = ["10.0.1.0/24"]
}

resource "azurerm_subnet" "db" {
  name                 = "db"
  resource_group_name  = azurerm_resource_group.rg.name
  virtual_network_name = azurerm_virtual_network.vnet.name
  address_prefixes     = ["10.0.2.0/24"]

  delegation {
    name = "postgres"
    service_delegation {
      name    = "Microsoft.DBforPostgreSQL/flexibleServers"
      actions = ["Microsoft.Network/virtualNetworks/subnets/join/action"]
    }
  }
}

resource "azurerm_subnet" "influx" {
  count                = local.p2
  name                 = "influx"
  resource_group_name  = azurerm_resource_group.rg.name
  virtual_network_name = azurerm_virtual_network.vnet.name
  address_prefixes     = ["10.0.3.0/24"]
}

# ------------------------------------------------------------------ NSG app
resource "azurerm_network_security_group" "app" {
  name                = "nsg-app"
  location            = azurerm_resource_group.rg.location
  resource_group_name = azurerm_resource_group.rg.name

  security_rule {
    name                       = "permitir-app-8000"
    priority                   = 100
    direction                  = "Inbound"
    access                     = "Allow"
    protocol                   = "Tcp"
    source_port_range          = "*"
    destination_port_range     = "8000"
    source_address_prefix      = "Internet"
    destination_address_prefix = "*"
  }

  security_rule {
    name                       = "permitir-ssh-mi-ip"
    priority                   = 110
    direction                  = "Inbound"
    access                     = "Allow"
    protocol                   = "Tcp"
    source_port_range          = "*"
    destination_port_range     = "22"
    source_address_prefixes    = var.mis_ips
    destination_address_prefix = "*"
  }

  dynamic "security_rule" {
    for_each = var.habilitar_p2 ? [1] : []
    content {
      name                       = "permitir-mqtt-1883"
      priority                   = 120
      direction                  = "Inbound"
      access                     = "Allow"
      protocol                   = "Tcp"
      source_port_range          = "*"
      destination_port_range     = "1883"
      source_address_prefix      = "Internet"
      destination_address_prefix = "*"
    }
  }

  dynamic "security_rule" {
    for_each = var.habilitar_p2 ? [1] : []
    content {
      name                       = "permitir-grafana-mis-ips"
      priority                   = 130
      direction                  = "Inbound"
      access                     = "Allow"
      protocol                   = "Tcp"
      source_port_range          = "*"
      destination_port_range     = "3000"
      source_address_prefixes    = var.mis_ips
      destination_address_prefix = "*"
    }
  }
}

resource "azurerm_subnet_network_security_group_association" "app" {
  subnet_id                 = azurerm_subnet.app.id
  network_security_group_id = azurerm_network_security_group.app.id
}

# ------------------------------------------------------------------ NSG db
resource "azurerm_network_security_group" "db" {
  name                = "nsg-db"
  location            = azurerm_resource_group.rg.location
  resource_group_name = azurerm_resource_group.rg.name

  security_rule {
    name                       = "permitir-5432-desde-app"
    priority                   = 100
    direction                  = "Inbound"
    access                     = "Allow"
    protocol                   = "Tcp"
    source_port_range          = "*"
    destination_port_range     = "5432"
    source_address_prefix      = "10.0.1.0/24"
    destination_address_prefix = "*"
  }

  security_rule {
    name                       = "negar-resto-vnet"
    priority                   = 200
    direction                  = "Inbound"
    access                     = "Deny"
    protocol                   = "*"
    source_port_range          = "*"
    destination_port_range     = "*"
    source_address_prefix      = "VirtualNetwork"
    destination_address_prefix = "*"
  }
}

resource "azurerm_subnet_network_security_group_association" "db" {
  subnet_id                 = azurerm_subnet.db.id
  network_security_group_id = azurerm_network_security_group.db.id
}

# ------------------------------------------------------------------ NSG influx (P2)
resource "azurerm_network_security_group" "influx" {
  count               = local.p2
  name                = "nsg-influx"
  location            = azurerm_resource_group.rg.location
  resource_group_name = azurerm_resource_group.rg.name

  security_rule {
    name                       = "permitir-8086-desde-app"
    priority                   = 100
    direction                  = "Inbound"
    access                     = "Allow"
    protocol                   = "Tcp"
    source_port_range          = "*"
    destination_port_range     = "8086"
    source_address_prefix      = "10.0.1.0/24"
    destination_address_prefix = "*"
  }

  security_rule {
    name                       = "permitir-ssh-desde-app"
    priority                   = 110
    direction                  = "Inbound"
    access                     = "Allow"
    protocol                   = "Tcp"
    source_port_range          = "*"
    destination_port_range     = "22"
    source_address_prefix      = "10.0.1.0/24"
    destination_address_prefix = "*"
  }

  security_rule {
    name                       = "negar-resto-vnet"
    priority                   = 200
    direction                  = "Inbound"
    access                     = "Deny"
    protocol                   = "*"
    source_port_range          = "*"
    destination_port_range     = "*"
    source_address_prefix      = "VirtualNetwork"
    destination_address_prefix = "*"
  }
}

resource "azurerm_subnet_network_security_group_association" "influx" {
  count                     = local.p2
  subnet_id                 = azurerm_subnet.influx[0].id
  network_security_group_id = azurerm_network_security_group.influx[0].id
}

# ------------------------------------------------------------------ IP publica y vm-iaot
resource "azurerm_public_ip" "ip" {
  name                = "ip-iaot"
  location            = azurerm_resource_group.rg.location
  resource_group_name = azurerm_resource_group.rg.name
  sku                 = "Standard"
  allocation_method   = "Static"
}

resource "azurerm_network_interface" "vm_iaot" {
  name                = "nic-vm-iaot"
  location            = azurerm_resource_group.rg.location
  resource_group_name = azurerm_resource_group.rg.name

  ip_configuration {
    name                          = "ipconfig1"
    subnet_id                     = azurerm_subnet.app.id
    private_ip_address_allocation = "Dynamic"
    public_ip_address_id          = azurerm_public_ip.ip.id
  }
}

resource "azurerm_linux_virtual_machine" "vm_iaot" {
  name                            = "vm-iaot"
  location                        = azurerm_resource_group.rg.location
  resource_group_name             = azurerm_resource_group.rg.name
  size                            = var.talla_vm
  admin_username                  = var.usuario_vm
  disable_password_authentication = true
  network_interface_ids           = [azurerm_network_interface.vm_iaot.id]

  admin_ssh_key {
    username   = var.usuario_vm
    public_key = file(pathexpand(var.llave_ssh_publica))
  }

  os_disk {
    caching              = "ReadWrite"
    storage_account_type = "StandardSSD_LRS"
  }

  source_image_reference {
    publisher = "Canonical"
    offer     = "ubuntu-24_04-lts"
    sku       = "server"
    version   = "latest"
  }
}

# ------------------------------------------------------------------ vm-influx (P2, sin IP publica)
resource "azurerm_network_interface" "vm_influx" {
  count               = local.p2
  name                = "nic-vm-influx"
  location            = azurerm_resource_group.rg.location
  resource_group_name = azurerm_resource_group.rg.name

  ip_configuration {
    name                          = "ipconfig1"
    subnet_id                     = azurerm_subnet.influx[0].id
    private_ip_address_allocation = "Static"
    private_ip_address            = local.ip_influx
  }
}

resource "azurerm_linux_virtual_machine" "vm_influx" {
  count                           = local.p2
  name                            = "vm-influx"
  location                        = azurerm_resource_group.rg.location
  resource_group_name             = azurerm_resource_group.rg.name
  size                            = var.talla_vm
  admin_username                  = var.usuario_vm
  disable_password_authentication = true
  network_interface_ids           = [azurerm_network_interface.vm_influx[0].id]

  admin_ssh_key {
    username   = var.usuario_vm
    public_key = file(pathexpand(var.llave_ssh_publica))
  }

  os_disk {
    caching              = "ReadWrite"
    storage_account_type = "StandardSSD_LRS"
  }

  source_image_reference {
    publisher = "Canonical"
    offer     = "ubuntu-24_04-lts"
    sku       = "server"
    version   = "latest"
  }
}

# ------------------------------------------------------------------ PostgreSQL privado
# La zona DNS no puede llamarse "<servidor>.postgres.database.azure.com" (error
# PrivateDnsZoneNameNotValid), por eso se usa iaotdb.postgres.database.azure.com.
resource "azurerm_private_dns_zone" "pg" {
  name                = local.zona_dns
  resource_group_name = azurerm_resource_group.rg.name
}

resource "azurerm_private_dns_zone_virtual_network_link" "pg" {
  name                  = "enlace-vnet-iaot"
  resource_group_name   = azurerm_resource_group.rg.name
  private_dns_zone_name = azurerm_private_dns_zone.pg.name
  virtual_network_id    = azurerm_virtual_network.vnet.id
}

resource "azurerm_postgresql_flexible_server" "pg" {
  name                          = local.servidor_bd
  resource_group_name           = azurerm_resource_group.rg.name
  location                      = azurerm_resource_group.rg.location
  version                       = "16"
  sku_name                      = "B_Standard_B1ms"
  storage_mb                    = 32768
  administrator_login           = var.admin_bd
  administrator_password        = var.clave_bd
  delegated_subnet_id           = azurerm_subnet.db.id
  private_dns_zone_id           = azurerm_private_dns_zone.pg.id
  public_network_access_enabled = false

  # Azure elige la zona de disponibilidad; sin esto Terraform queria "corregirla" cada vez.
  lifecycle {
    ignore_changes = [zone]
  }

  depends_on = [azurerm_private_dns_zone_virtual_network_link.pg]
}

resource "azurerm_postgresql_flexible_server_database" "iaot" {
  name      = "iaot"
  server_id = azurerm_postgresql_flexible_server.pg.id
  collation = "en_US.utf8"
  charset   = "UTF8"
}

# ------------------------------------------------------------------ archivos que usan los scripts desplegar_*.sh
resource "local_file" "infra_env" {
  filename        = abspath("${path.module}/../../practica-1/nube/infra.env")
  file_permission = "0600"
  content         = <<-EOT
    GRUPO=${azurerm_resource_group.rg.name}
    IP_PUBLICA=${azurerm_public_ip.ip.ip_address}
    USUARIO_VM=${var.usuario_vm}
    HOST_BD=${azurerm_postgresql_flexible_server.pg.fqdn}
    ADMIN_BD=${var.admin_bd}
  EOT
}

resource "local_file" "infra_p2_env" {
  count           = local.p2
  filename        = abspath("${path.module}/../../practica-2/nube/infra_p2.env")
  file_permission = "0600"
  content         = "IP_INFLUX=${local.ip_influx}\n"
}
