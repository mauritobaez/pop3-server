# Trabajo Práctico Protocolos de Comunicación
# Grupo 8

## Integrantes
+ Shih Federico, 62293
+ Rupnik Franco, 62032
+ Manzur Matías, 62498
+ Báez Mauro, 61747

# POP3
Siguiendo el RFC 1939, se implementó un servidor POP3. 

# PEEP (Protocolo de Monitoreo)
Diseñando nuestro propio protocolo, implementamos una manera sencilla de realizar modificaciones en el POP3 server.
Se pueden encontrar en el archivo `rfc.txt` las especificaciones de dicho protocolo.

Para una utilización más sencilla de PEEP, el administrador puede utilizar el cliente PEEP que desarrollamos el cual
sirve de traductor entre el usuario y PEEP, haciendo más legibles los mensajes de error al igual que la entrada de los
distintos comandos del protocolo.


# Materiales
Se puede encontrar un informe acerca del desarrollo del trabajo práctico en el archivo `informe-grupo-8.txt`. El código fuente del servidor POP3 y PEEP puede ser encontrado en el directorio src, mientras que el código del cliente PEEP puede ser encontrado en el directorio peep-client.


## Guía de Instalación y Uso
1. Realizar, en la carpeta raíz del proyecto, make all CC=gcc o make all CC=clang para compilar tanto el servidor POP3 y PEEP como el cliente PEEP.
   
2. Los binarios se encontrarán en la misma carpeta raíz. 
   
3. Ejecutar los binarios con los flags deseados. Ejecutar algo del estilo ./pop3d -u user:password -u user2:password2 -m ../maildirectory, esto comenzará el servidor POP3 y creará dos usuarios, user y user2. Además, establece el directorio de donde se obtienen los emails de los distintos usuarios como ../maildirectory. Más usuarios pueden ser añadidos a través de una conexión PEEP (utilizando el cliente PEEP).

4. Se puede utilizar cualquier MUA para acceder al servidor. Una forma rápida de acceder desde la terminal es mediante curl o directamente con netcat como nc -C localhost 1110 (puerto default).

5. Para acceder a las opciones de administrador (PEEP), leer la siguiente parte de este documento. Si se quiere establecer las credenciales del administrador, utilizar el argumento --peep-admin <admin-username>:<admin-password>


### Servidor
Argumentos posibles al hacer ./pop3d. Cada uno tiene su versión abreviada (`-u`) y completa (`--user`), pero los parámetros que estos reciben son los mismos más alla de qué version se utilice. 
+ `-u, --user <username>:<password>`     Genera un usuario con nombre `username` y contraseña `password`.
+ `-m, --mailbox <maildir>`    Establece el directorio de emails.
+ `-a, --peep-admin <admin-username>:<admin-password>`     Establece las credenciales del administrador.
+ `-p, --pop3-port <port>`  Establece el puerto en el que estará el socket pasivo de POP3.
+ `-P, --peep-port <port>`  Establece el puerto en el que estará el socket pasivo de PEEP.
+ `-t, --transform '<transformation>'` Establece qué transformación será utilizada. Se debe indicar un comando a ejecutar que recibirá el mail como argumento. (ej.: 'tac')
+ `-T, --timeout <timeout_value>` Establece el tiempo máximo que espera entre interacciones de un usuario antes de desconectarlo. (0 indica que no hay timeout).


### Cliente
Corriendo ./peepclient, los argumentos disponibles son los siguientes:
+ `--help`  Imprime los argumentos a establecer para PEEP
+ `-address <address>`    IP a utilizar.
+ `-port <puerto>`  Puerto a utilizar.
+ `-user <name> -pass <pass>`   Credenciales del admin, por default son root y root.

