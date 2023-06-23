# Stress testing

Para realizar una versión cruda de stress testing, utilizamos las librerías
- node-pop3
- netcat

de Javascript.

# Precondiciones
- Tener Node versión 16.14.0
- Tener NPM versión 9.6.7

# Guia de ejecucion
1. ``npm install`` para instalar dependencias
2. ``npm start <cantidad de conexiones>`` para generar RETRs

Debido a que se excede el scope del trabajo practico, el script te obliga a modificar las variables manualmente.

El script crea mediante Netcat n cantidad de usuarios mediante PEEP. Estos usuarios van de 0:n-1. Se supone que previamente en tu maildir estan todos estos usuarios, por lo que hay un script de directories.sh que popula tu maildir con tales usuarios.