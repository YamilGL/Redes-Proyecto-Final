## Redes-proyecto-Final
# Descripción
Este proyecto trata de entrenar una red neuronal de manera distribuida, utilizando protocolos de conexión como TCP y UDP 
Implementando checksum,n-seq, keep-alive. La red neuronal aprenderá a jugar el juego TIC TAC

# Compilar Server
Servidor:./Serv
# Compilar Red Neuronal
Cliente 1: ./RedNeuro 192.168.0.2 9001
Cliente 2: ./RedNeuro 192.168.0.3 9002
Cliente 3: ./RedNeuro 192.168.0.4 9003
Cliente 4: ./RedNeuro 192.168.0.5 9004
# Compilar Terminal
Terminal:./Terminal
