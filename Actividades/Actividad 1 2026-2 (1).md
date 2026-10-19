## Inteligencia Artificial aplicada a Internet de las Cosas Práctica 1-Servidor web para un proyecto de IoT Especialización en inteligencia artificial Jhorman A Villanueva Vivas

## Objetivos.

- Configurar un objeto inteligente para que se conecte a internet y transmita datos recolectados por sensores en tiempo real a un servidor.

- Implementar un servidor web que reciba, procese y almacene datos enviados por un dispositivo inteligente.

Recursos por utilizar:

- ESP32

- Sensor DHT11-22

Procedimiento:

## Primer Parte

- 1. Determine un caso de uso donde se monitoree temperatura y humedad. Puede realizarlo de acuerdo con la empresa donde labora:

- a. Ejemplo: en un cultivo de invernadero de tomate la temperatura y humedad es muy importante para maximizar el crecimiento y salud del cultivo…

- 2. Configure una aplicación web en un servidor en Ubuntu. Puede usar el lenguaje y framework que desee

- a. Python (Flask, Django, FastAPI)

- b. NodeJS

- c. PHP

- d. Java

- 3. Modifique el código del ESP32 para enviar datos a la aplicación web.

- 4. Los datos medidos por los sensores se deben almacenar en un csv y en una base de datos.

## Segunda parte

- 5. Realice la misma implementación del servidor, pero ahora en la nube en una máquina virtual. El servidor donde corre la aplicación web debe estar en una


subred pública y la base de datos debe estar en una subred privada. Tener en cuenta:

- a. Asignar una IP elástica.

- b. Configuración correcta de los grupos de seguridad.

- 6. Alojar el proyecto en un repositorio de Github (un integrante del equipo comparte el enlace)

- 7. Realizar presentación frente a los compañeros.

## Criterios de evaluación:

- Funcionalidad: los datos medidos por los sensores se deben verificar en el recurso de almacenamiento (archivo csv o base de datos).

- Comunicación: el dispositivo mantiene una conexión estable y envía los datos a través de un protocolo de comunicación.

- Configuración del servidor web: el servidor web en configurando local y posteriormente en la nube de AWS. Es accesible de forma remota y está configurado para recibir los datos del esp32 y almacenarlos en un archivo CSV y base de datos.
