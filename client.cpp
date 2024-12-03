#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/ioctl.h>

#include <iostream>
#include <string>
#include <iomanip>
#include <thread>

using namespace std;

string tablero;
// X O
char ficha;
// 1 mi turno
// 0 turno del oponente
char turno;

void imprimirTablero() {
    for (int i = 0; i < tablero.size(); ++i) {
        if(tablero[i] == '0'){
            cout<<" "<<i + 1<<" ";
        }
        else{
            cout<<" "<<tablero[i]<<" ";
        }
        if (i % 3 == 2) { 
            if (i < tablero.size()) {
                cout<<"\n-----------\n";
            }
        } else {
            cout<<"|"; 
        }
    }
    cout<<endl;
}

void continuarJuego(char parsed[255], int SocketClient){
    int indice = 0;
    string posicion;

    
    cout<<"Coloque la posicion de su ficha (1-9): "<<endl;
    imprimirTablero();

    while(1){
        cout<<endl;
        getline(cin, posicion);
        
        indice = atoi(posicion.c_str()) - 1;

        if (indice >= 0 && indice < tablero.size() && tablero[indice] == '0') break;
        else cout<<"Posicion invalida o ya ocupada. Intente de nuevo."<<endl;
        
        cout<<"Coloque la posicion de su ficha (1-9): "<<endl;
    }    

    parsed[0] = 'T';    
    int i = 1;

    strcpy(parsed + i, posicion.c_str());
    i++;

    write(SocketClient, parsed, i);
}

void reading(int SocketClient){
    char buff[255];
    int n,i; 
    string name;
    string msgs;

    while(1){
        bzero(buff,255);
        n = read(SocketClient, buff, 255);
        string msg(buff);
        if(n == 0) {
            printf("Disconnecting ...\n");
            shutdown(SocketClient, SHUT_RDWR);
            close(SocketClient);
            exit(0);
            return;
        }
        i = 0;
        if(buff[i] != 'U'){
            buff[n] = '\0';
            i++;
        }
        switch (msg[0]){ 
            case 'T':{
                ficha = msg[1];
                tablero = msg.substr(2, 9);
                string ganador = msg.substr(11, 1);
                cout<<tablero<<endl;
                cout<<ganador<<endl;

                cout<<endl;
                if(ganador == "1"){
                    tablero.clear();
                    printf("[Server - Juego]: Ganaste! \n");
                    break;
                }
                else if(ganador == "2"){
                    tablero.clear();
                    printf("[Server - Juego]: Gana IA!\n");
                    break;
                }
                else{
                    printf("[Server - Juego]: Es tu turno.\n");
                    printf("Tu ficha es: %c\n", ficha);
                    break;
                }
            }
            default:{
                printf("\n[Server - Notificaciones]: [%s]\n", msg.c_str());
                break;
            }
        }
    }
    shutdown(SocketClient, SHUT_RDWR);
    close(SocketClient);
}

void writing(int SocketClient){
    char parsed[255];
    string accion;
    
    while(1){
        bzero(parsed,255);
        if(tablero.size() == 9){    
            sleep(1.5);
            continuarJuego(parsed, SocketClient);
        }
        else{
            cout<<"Empezar juego? ";
            getline (cin,accion);
            // Cerrar programa
            if(accion == "end") {
                shutdown(SocketClient, SHUT_RDWR);
                close(SocketClient);
                exit(0);
                return;
            }
            parsed[0] = 'J';
            tablero = "000000000";
            write(SocketClient, parsed, 1);
        }        
    }
}
 
int main(void){
    struct sockaddr_in stSockAddr;
    int SocketClient = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
    int n;
    int Res;

    if(-1 == SocketClient){
        perror("can not create socket");
        exit(EXIT_FAILURE);
    }

    memset(&stSockAddr, 0, sizeof(struct sockaddr_in));

    stSockAddr.sin_family = AF_INET;
    stSockAddr.sin_port = htons(8080);
    Res = inet_pton(AF_INET, "18.225.56.177", &stSockAddr.sin_addr);

    string name, pass;
    char buffer[255];

    connect(SocketClient, (const struct sockaddr *)&stSockAddr, sizeof(struct sockaddr_in));

    while(1){
        cout<<"Escriba su user name: ";
        getline (cin,name);
        cout<<"Escriba su password: ";
        getline (cin,pass);

        char parsed[255];

        string s_name = to_string(name.size());
        string s_pass = to_string(pass.size());

        if(s_name.size() == 1) s_name = "0"+s_name;
        if(s_pass.size() == 1) s_pass = "0"+s_pass;

        parsed[0] = 'L';
        int i = 1; 

        strcpy(parsed + i, s_name.c_str());
        i += s_name.size();

        strcpy(parsed + i, name.c_str());
        i += name.size();

        strcpy(parsed + i, s_pass.c_str());
        i += s_pass.size();

        strcpy(parsed + i, pass.c_str());
        i += pass.size();

        write(SocketClient, parsed, i);

        bzero(buffer,255);
        read(SocketClient, buffer, 255);
        printf("Status: [%s]\n", buffer);
        if(buffer[0] == 'C') break;
    }

    thread (writing,SocketClient).detach();
    thread (reading,SocketClient).detach();

    while(1){}   
    return 0;
}
