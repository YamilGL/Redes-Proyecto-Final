#include <iostream>
#include <thread>
#include <map>
#include <vector>
#include <string>
#include <sstream>
#include <fstream>
#include <mutex>
#include <random>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/socket.h>
#include <algorithm>
#include <chrono>
#include <cstring>
#include <sstream>
#include <iomanip> 
#include "tresEnRaya.h"
#define MAXLINE 1000


// Estructura para envío de mensajes UDP
struct envioMensajesUDP {
    char Comando[1];     // Comando que ira al inicio del mensaje
    uint32_t seq_num;    // Número de secuencia
    uint16_t checksum;   // Checksum
    char dataMensaje[1017]; // Datos (1024 - 4 bytes de secuencia - 2 bytes de checksum - 1 comando)
};

// Estructura para recibir mensajes UDP
struct recibirMensajesUDP {
    char Comando[1];
    uint32_t seq_num;
    uint16_t checksum;
    char dataMensaje[1017];
};

struct ServerConfig {
    std::vector<int> nombres;  // client_id
    std::vector<int> ips;  // client_socket
    std::vector<bool> elegido;
};

TresEnRaya tresEnRaya;

std::map<std::string, int> terminal_sockets;
std::map<std::string, sockaddr_in> client_udp_sockets;
std::multimap<int, std::string> DataAprendisaje;
ServerConfig server_config;
std::mutex config_mutex;
std::mutex message_count_mutex;
bool keep_alive_active = false;
bool first_message_received = false;
int message_count = 0;
int cantidad_max_Usuarios = 4;

void handle_tcp_client(int client_socket, std::string client_ip, int udp_socket);
void handle_udp_connection(int udp_socket);

void udp_send_function(int udp_socket, const std::string &message, const sockaddr_in *client_addr, char comando, uint32_t seq_num);

void tcp_broadcast_function(const std::string &message);
void update_selected_client();
void load_data_aprendisaje(const std::string &filename);
void print_data_aprendisaje();
void distribute_data_to_clients(int udp_socket);
void keep_alive_monitor();


int getIdClientes(int ids);
std::string deParsingTablero(std::string parsedTablero);
std::string parsingTablero(std::string tablero);
void jugadaIA(int idPlayer, std::string tabla);
void login_client(int sockCli, char buff[MAXLINE], std::string message);
void start_game(int sockCli);
void playing();
void parseGame(int sockCli, char ficha, string tabla, char ganador);
int getTableroClientes(int socketCli);
bool hayGanador(string tablero, char turno);
string getNameSender(int sockCli);
int getMainServer();
uint16_t calcularChecksum(const char *data, size_t length);

int main() {
    load_data_aprendisaje("data.csv");
    
    // Setup TCP server
    int tcp_socket = socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in tcp_server_addr;
    tcp_server_addr.sin_family = AF_INET;
    tcp_server_addr.sin_addr.s_addr = INADDR_ANY;
    tcp_server_addr.sin_port = htons(8080);

    bind(tcp_socket, (struct sockaddr *)&tcp_server_addr, sizeof(tcp_server_addr));
    listen(tcp_socket, 5);

    // Setup UDP server
    int udp_socket = socket(AF_INET, SOCK_DGRAM, 0);
    sockaddr_in udp_server_addr;
    udp_server_addr.sin_family = AF_INET;
    udp_server_addr.sin_addr.s_addr = INADDR_ANY;
    udp_server_addr.sin_port = htons(8081);

    bind(udp_socket, (struct sockaddr *)&udp_server_addr, sizeof(udp_server_addr));

    std::thread udp_thread(handle_udp_connection, udp_socket);
    udp_thread.detach();

    // Crear y manejar el hilo de keep-alive monitor en el main
    std::thread keep_alive_thread(keep_alive_monitor);
    keep_alive_thread.detach();

    while (true) {
        sockaddr_in client_addr;
        socklen_t client_addr_len = sizeof(client_addr);
        int new_socket = accept(tcp_socket, (struct sockaddr *)&client_addr, &client_addr_len);
        std::string client_ip = inet_ntoa(client_addr.sin_addr);

        std::thread tcp_thread(handle_tcp_client, new_socket, client_ip, udp_socket);
        tcp_thread.detach();
    }

    close(tcp_socket);
    close(udp_socket);

    return 0;
}

void handle_tcp_client(int client_socket, std::string client_ip, int udp_socket) {
    char buffer[MAXLINE];
    while (true) {
        int bytes_received = read(client_socket, buffer, sizeof(buffer));
        if (bytes_received > 0) {
            buffer[bytes_received] = '\0';
            std::string message(buffer);

            std::lock_guard<std::mutex> lock(config_mutex);
            switch (message[0]) {
                case '1': {
                    int client_id = server_config.nombres.size() + 1;
                    server_config.nombres.push_back(client_id);
                    server_config.ips.push_back(client_socket);
                    server_config.elegido.push_back(false);
                    terminal_sockets[client_ip] = client_socket;
                    std::cout << "TCP client connected: " << client_ip << " with ID " << client_id << std::endl;
                    if (server_config.nombres.size() == cantidad_max_Usuarios) {
                        update_selected_client();
                        keep_alive_active = true;
                        // Esperar 1 segundo antes de enviar el comando 'H'
                        std::this_thread::sleep_for(std::chrono::seconds(1));
                        // Enviar comando 'H' a todos los clientes TCP para activar el keep-alive
                        for (int socket : server_config.ips) {
                            std::string keep_alive_command = "H";
                            send(socket, keep_alive_command.c_str(), keep_alive_command.size(), 0);
                        }
                    }
                    break;
                }
                case 'H': {
                    std::lock_guard<std::mutex> count_lock(message_count_mutex);
                    message_count++;
                    first_message_received = true;
                    break;
                }
                case 'B': {
                    for (size_t i = 0; i < server_config.nombres.size(); ++i) {
                        std::cout << "Client ID " << server_config.nombres[i] << " - Socket: " << server_config.ips[i] << " - Elegido: " << server_config.elegido[i] << std::endl;
                    }
                    break;
                }
                case 'G': {
                    std::cout << " Caso G enviando por udp La data"<< std::endl;
                    
                    distribute_data_to_clients(udp_socket);
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                    tcp_broadcast_function(message);
            
                    
                    break;
                }
                case 'm': {
                    cout<<"mensaje recibido: "<<message<<endl;
                    send(getMainServer(), message.c_str(), message.size(), 0);
                    break;
                }
                case 'p': {
                    cout<<"MEDIA RECIBIDA: "<<message<<endl;
                    tcp_broadcast_function(message);
                    break;
                }
                case 'M': {
                    print_data_aprendisaje();
                    break;
                }
                case 'C': {
                    message[0] = 'G';
                    tcp_broadcast_function(message);
                    break;
                }
                case 'L' : {
                    login_client(client_socket, buffer, message);
                    break;
                }
                case 'J' : {
                    start_game(client_socket);
                    break;
                }
                case 'D': { 
                    for (const auto& [client_key, client_addr] : client_udp_sockets) {
                        udp_send_function(udp_socket, "", &client_addr, 'D', 0);  // Enviar comando 'D' a los clientes UDP
                    }
                    break;
                }
                case 'T' : {
                    // Juego | T ___ _ 
                    // T - idJugador de 3 bytes - posicion
                    if(client_socket == getMainServer()){
                        int id = std::stoi(message.substr(1, 3));
                        int posicion = std::stoi(message.substr(4, 1));
                        int player = getIdClientes(id);
                        char iaFicha = (tresEnRaya.ficha[player] == 'X')? 'O' : 'X';
                        tresEnRaya.movimiento(tresEnRaya.tablero[player], posicion, iaFicha);
                        // winner:
                        // 0 si no hay ganador
                        // 1 si gana Jugador
                        // 2 si gana IA
                        cout<<tresEnRaya.tablero[player]<<endl;
                        char winner = '0';
                        if(hayGanador(tresEnRaya.tablero[player], iaFicha)) winner = '2'; 
                        parseGame(tresEnRaya.socket[player], tresEnRaya.ficha[player], tresEnRaya.tablero[player], winner);
                        break;
                    }
                    // T posicion
                    else{
                        int posicion = std::stoi(message.substr(1, 2)) - 1;
                        int player = getTableroClientes(client_socket);
                        char iaFicha = (tresEnRaya.ficha[player] == 'X')? 'O' : 'X';

                        tresEnRaya.movimiento(tresEnRaya.tablero[player], posicion, tresEnRaya.ficha[player]);
                        if(hayGanador(tresEnRaya.tablero[player], tresEnRaya.ficha[player])) {
                            parseGame(client_socket, tresEnRaya.ficha[player], tresEnRaya.tablero[player], '1');
                            break;
                        }
                        jugadaIA(player, tresEnRaya.tablero[player]);
                        break;
                    }
                }
                default: {
                    std::cout << "Received TCP message: " << message << std::endl;
                    break;
                }
            }
        } else {
            close(client_socket);
            break;
        }
    }
}

void handle_udp_connection(int udp_socket) {
    while (true) {
        sockaddr_in client_addr;
        socklen_t client_addr_len = sizeof(client_addr);
        char buffer[MAXLINE];
        int bytes_received = recvfrom(udp_socket, buffer, sizeof(buffer), 0, (struct sockaddr *)&client_addr, &client_addr_len);
        if (bytes_received > 0) {
            buffer[bytes_received] = '\0';
            std::string client_ip = inet_ntoa(client_addr.sin_addr);
            std::lock_guard<std::mutex> lock(config_mutex);
            client_udp_sockets[client_ip + ":" + std::to_string(ntohs(client_addr.sin_port))] = client_addr;
            std::cout << "UDP client connected: " << client_ip << " on port " << ntohs(client_addr.sin_port) << std::endl;
        }
    }
}

// Function to send UDP messages
void udp_send_function(int udp_socket, const std::string &message, const sockaddr_in *client_addr, char comando, uint32_t seq_num) {
    envioMensajesUDP packet;
    packet.Comando[0] = comando;
    packet.seq_num = seq_num;
    strncpy(packet.dataMensaje, message.c_str(), sizeof(packet.dataMensaje) - 1);
    packet.dataMensaje[sizeof(packet.dataMensaje) - 1] = '\0'; // Null-terminate the string
    packet.checksum = calcularChecksum(packet.dataMensaje, sizeof(packet.dataMensaje));

    // Imprimir los datos de la estructura antes de enviar
    std::cout << "Preparing to send packet:" << std::endl;
    std::cout << "  Comando: " << packet.Comando[0] << std::endl;
    std::cout << "  Seq_num: " << packet.seq_num << std::endl;
    std::cout << "  Checksum: " << packet.checksum << std::endl;
    std::cout << "  DataMensaje: " << std::setw(50) << std::left << packet.dataMensaje << std::endl;
    std::cout << "  Client IP: " << inet_ntoa(client_addr->sin_addr) << ":" << ntohs(client_addr->sin_port) << std::endl;

    sendto(udp_socket, &packet, sizeof(packet), 0, (struct sockaddr *)client_addr, sizeof(*client_addr));
}

// Function to send TCP broadcast messages
void tcp_broadcast_function(const std::string &message) {
    for (const auto& socket : server_config.ips) {
        send(socket, message.c_str(), message.size(), 0);
        std::this_thread::sleep_for(std::chrono::milliseconds(100)); // Agregar un pequeño retraso entre envíos
    }
}

void update_selected_client() {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, cantidad_max_Usuarios - 1);

    int selected_index = dis(gen);
    for (size_t i = 0; i < server_config.elegido.size(); ++i) {
        server_config.elegido[i] = (i == selected_index);
    }

    // Send the names
    std::string names_message = "1";
    for (const auto& id : server_config.nombres) {
        names_message += std::to_string(id) + ",";
    }
    tcp_broadcast_function(names_message);

    // Send the IPs (which are actually the sockets)
    std::string ips_message = "2";
    for (const auto& socket : server_config.ips) {
        ips_message += std::to_string(socket) + ",";
    }
    tcp_broadcast_function(ips_message);

    // Send the selected
    std::string selected_message = "3";
    for (const auto& selected : server_config.elegido) {
        selected_message += (selected ? "1" : "0") + std::string(",");
    }
    tcp_broadcast_function(selected_message);
}

// Function to load data from CSV
void load_data_aprendisaje(const std::string &filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Error opening file: " << filename << std::endl;
        return;
    }

    std::string line;
    while (std::getline(file, line)) {
        std::istringstream iss(line);
        int key;
        std::string value;
        if (iss >> key) {
            std::getline(iss, value);
            value.erase(std::remove(value.begin(), value.end(), ' '), value.end());
            DataAprendisaje.insert({key, value});
        }
    }

    file.close();
}

// Function to print the data structure
void print_data_aprendisaje() {
    std::cout << "DataAprendisaje:" << std::endl;
    int first_count = 0;
    int second_count = 0;

    for (const auto& [key, value] : DataAprendisaje) {
        std::cout << "  " << key << " => " << value << std::endl;
        first_count++;
        if (!value.empty()) {
            second_count++;
        }
    }

    std::cout << "Total elements in first: " << first_count << std::endl;
    std::cout << "Total elements in second (non-empty): " << second_count << std::endl;
}

void distribute_data_to_clients(int udp_socket) {
    int num_clients = server_config.nombres.size();
    if (num_clients == 0) {
        std::cerr << "No clients connected to distribute data" << std::endl;
        return;
    }

    int part_size = DataAprendisaje.size() / num_clients;
    auto it = DataAprendisaje.begin();
    uint32_t seq_num = 0;

    for (const auto& [client_key, client_addr] : client_udp_sockets) {
        std::string key_message = "K";
        std::string value_message = "L";
        bool first_key = true;
        bool first_value = true;

        for (int i = 0; i < part_size && it != DataAprendisaje.end(); ++i, ++it) {
            if (!first_key) {
                key_message += ",";
                value_message += ",";
            }
            key_message += std::to_string(it->first);
            value_message += it->second;
            first_key = false;
            first_value = false;
        }

        udp_send_function(udp_socket, key_message, &client_addr, 'K', seq_num);
        udp_send_function(udp_socket, value_message, &client_addr, 'L', seq_num);
        seq_num++;
    }

    // If there are remaining elements, send them to the last client
    if (it != DataAprendisaje.end()) {
        std::string key_message = "K";
        std::string value_message = "L";
        bool first_key = true;
        bool first_value = true;

        while (it != DataAprendisaje.end()) {
            if (!first_key) {
                key_message += ",";
                value_message += ",";
            }
            key_message += std::to_string(it->first);
            value_message += it->second;
            first_key = false;
            first_value = false;
            ++it;
        }

        auto last_client_addr = &client_udp_sockets.rbegin()->second;
        udp_send_function(udp_socket, key_message, last_client_addr, 'K', seq_num);
        udp_send_function(udp_socket, value_message, last_client_addr, 'L', seq_num);
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
     // Send 'Z' message to indicate end of data transmission
    for (const auto& [client_key, client_addr] : client_udp_sockets) {
        udp_send_function(udp_socket, "", &client_addr, 'Z', seq_num);
    }
}

// Keep-alive monitor function
void keep_alive_monitor() {
    while (true) {
        if (keep_alive_active) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            if (first_message_received) {
                std::this_thread::sleep_for(std::chrono::seconds(5));
                std::lock_guard<std::mutex> lock(message_count_mutex);
                std::cout << "Mensajes recibidos en el lapso del timeout: " << message_count << std::endl;
                message_count = 0;
                first_message_received = false;
            }
        }
    }
}

void notification(string conf, int sockCli){
    write(sockCli, conf.c_str(), conf.length());
}

void login_client(int sockCli, char buff[MAXLINE], std::string message){
    string parsed;
    parsed[0] = buff[0];

    // Parsear el nombre
    int size_name = std::stoi(message.substr(1, 2)); // Asumiendo que el tamaño del nombre está en los caracteres 4 y 5
    std::string name = message.substr(3, size_name);

    // Parsear la contraseña
    int size_pass = std::stoi(message.substr(3 + size_name, 2)); // Asumiendo que el tamaño de la contraseña está después del nombre
    std::string pass = message.substr(5 + size_name, size_pass);

    // Construir el string 'parsed'
    parsed += message[0]; // 'T'
    parsed += message.substr(1, 2); // tamaño del nombre
    parsed += name; // nombre
    parsed += message.substr(3 + size_name, 2); // tamaño de la contraseña
    parsed += pass; // contraseña
    
    int player;
    if(tresEnRaya.id.size() == 0) player = -1;
    else player = getTableroClientes(sockCli);

    if (tresEnRaya.id.size() > 0 && tresEnRaya.socket[player] != 0) {
        string conf = "Usuario Conectado en otra terminal.";
        write(sockCli, conf.c_str(), conf.length());
        return;
    }

    if (player == -1) {
        tresEnRaya.jugador.push_back(name);
        tresEnRaya.pass.push_back(pass);
        tresEnRaya.socket.push_back(sockCli);
        tresEnRaya.tablero.push_back("000000000");
        tresEnRaya.id.push_back(tresEnRaya.id.size() + 1);

        cout<<"Chiste + "<<tresEnRaya.id[0]<<endl;

        printf("MSG: [%s] - Size: [%d] \n", parsed.c_str(), parsed.length());

        string conf = "Conectado!";
        write(sockCli, conf.c_str(), conf.length());
        return;
    }

    if (pass != tresEnRaya.pass[player]) {
        string msg_error = "Intente de nuevo.";
        write(sockCli, msg_error.c_str(), msg_error.size());
        return;
    }

    tresEnRaya.socket[player] = sockCli;

    printf("MSG: [%s] - Size: [%d] \n", parsed.c_str(), parsed.length());

    std::string conf = "Conectado!";
    write(sockCli, conf.c_str(), conf.length());
}

void jugadaIA(int player, std::string tabla){
    int socketMain = getMainServer();

    ostringstream stream;
    stream << setw(3) << setfill('0') << tresEnRaya.id[player];
    string idClienteParsed = stream.str(); 

    std::string tabla_parseada = parsingTablero(tabla);
    std::string msg_parsed  = "T" + idClienteParsed + tabla_parseada;
    printf("MSG: [%s] - Size: [%ld] \n", msg_parsed.c_str(), msg_parsed.size());
    write(socketMain, msg_parsed.c_str(), msg_parsed.size());

    msg_parsed = "Main: [Turno de IA]";
    notification(msg_parsed, tresEnRaya.socket[player]);
}

void start_game(int sockCli){
    int player = getTableroClientes(sockCli);
    if(tresEnRaya.id[player] == 0) {
        cout<<"error"<<endl;
        return;
    }

    srand(time(0));

    int turnoRandom = rand() % 2;
    char fi = (turnoRandom == 0) ? 'X' : 'O';
    tresEnRaya.ficha.push_back(fi);
    cout<<tresEnRaya.ficha[player]<<endl;

    if(tresEnRaya.ficha[player] == 'O') {
        jugadaIA(player, tresEnRaya.tablero[player]);
    }
    else{  
        parseGame(sockCli, tresEnRaya.ficha[player], tresEnRaya.tablero[player], '0');
    }
}

void parseGame(int sockCli, char ficha, string tabla, char ganador){
    char parsed[MAXLINE];
    parsed[0] = 'T';
    int i = 1;     
    
    parsed[i++] = ficha;

    strcpy(parsed + i, tabla.c_str());
    i+=tabla.size();

    parsed[i++] = ganador;
    
    write(sockCli, parsed, i);
    printf("MSG: [%s] - Size: [%d] \n", parsed, i);
}

int getTableroClientes(int socketCli) {
    for (int i = 0; i<tresEnRaya.socket.size(); i++) {
        if ((tresEnRaya.socket[i] == socketCli)) 
            return i; 
    }
    return -1;
}

int getIdClientes(int ids) {
    for (int i = 0; i<tresEnRaya.id.size(); i++) {
        if ((tresEnRaya.id[i] == ids)) 
            return i; 
    }
    return -1;
}

bool hayGanador(string tablero, char turno) {
    for (int i = 0; i < 9; i += 3) {
        if (tablero[i] == turno && tablero[i] == tablero[i + 1] && tablero[i] == tablero[i + 2]) 
            return true;
    }
    for (int i = 0; i < 3; ++i) {
        if (tablero[i] == turno && tablero[i] == tablero[i + 3] && tablero[i] == tablero[i + 6]) 
            return true;
    }
    if (tablero[0] == turno && tablero[0] == tablero[4] && tablero[0] == tablero[8]) 
        return true;
    if (tablero[2] == turno && tablero[2] == tablero[4] && tablero[2] == tablero[6]) 
        return true;
    return false;
}

int getMainServer() {
    for (size_t i = 0; i < server_config.nombres.size(); ++i) {
        if(server_config.elegido[i]) return server_config.ips[i]; 
    }
    return 0;
}

std::string parsingTablero(std::string tablero) {
    std::string vacio(9, '1');
    std::string X_(9, '0');
    std::string O_(9, '0');

    for (int i = 0; i < 9; ++i) {
        if (tablero[i] == 'X') {
            vacio[i] = '0';
            X_[i] = '1';
        }
        else if (tablero[i] == 'O') {
            vacio[i] = '0';
            O_[i] = '1';
        }
    }

    return vacio + X_ + O_;
}

std::string deParsingTablero(std::string parsedTablero) {
    std::string tablero(9, '0');

    for (int i = 0; i < 9; ++i) {
        if (parsedTablero[9 + i] == '1') {
            tablero[i] = 'X';
        }
        else if (parsedTablero[18 + i] == '1') {
            tablero[i] = 'O';
        }
    }

    return tablero;
}

uint16_t calcularChecksum(const char *data, size_t length) {
    uint32_t sum = 0;
    for (size_t i = 0; i < length; ++i) {
        sum += static_cast<uint8_t>(data[i]);
    }
    while (sum >> 16) {
        sum = (sum & 0xFFFF) + (sum >> 16);
    }
    return ~sum;
}