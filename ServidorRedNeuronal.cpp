#include <iostream>
#include <thread>
#include <vector>
#include <string>
#include <map>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sstream>
#include <mutex> 
#include <chrono>
#include <iomanip>
#include "tictactoe.h"


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
struct ClientConfig {
    std::vector<std::string> nombres;
    std::vector<int> ips;
    std::vector<bool> elegido;
};

std::multimap<int, std::string> DataAprendisaje;
std::mutex data_mutex;
std::mutex seq_num_mutex;
std::map<std::string, uint32_t> expected_seq_num;

std::vector<std::vector<double>> boards;
std::vector<int> nextMoves;
std::map<double, std::vector<double>> target;
std::vector<double> result;
std::vector<double> bestPlay;
std::vector<double> sftmxResult;
std::vector<double> media_calculada = {0,0,0,0,0,0,0,0,0};
int count_output_rn_recibido=0;
int generacion = 0;
std::vector<double> sftmxBestPlay;
Perceptron MLP = buildPerceptron();

ClientConfig client_config;
std::map<int, sockaddr_in> client_udp_sockets;
std::vector<int> received_keys;
std::vector<std::string> received_values;
bool keep_alive_active = false;

uint16_t calcularChecksum(const char *data, size_t length);
void handle_tcp_connection(int tcp_socket);
void handle_udp_connection(int udp_socket);
void process_udp_packet(const recibirMensajesUDP &packet, const std::string &client_ip);

void print_client_config();
void print_data_aprendisaje(); //  función para imprimir el map de DataAprendisaje
void send_keep_alive(int tcp_socket);
bool compare_indices(int a, int b) {
    return sftmxBestPlay[a] > sftmxBestPlay[b]; // Comparación para ordenar según los valores en vec
}

int main(int argc, char *argv[]) {
    if (argc < 3) {
        std::cerr << "Usage: " << argv[0] << " <udp_ip> <udp_port>" << std::endl;
        return 1;
    }

    const char* udp_ip = argv[1];
    int udp_port = std::stoi(argv[2]);

    // Setup TCP client
    int tcp_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (tcp_socket < 0) {
        std::cerr << "Error creating TCP socket" << std::endl;
        return 1;
    }
    sockaddr_in tcp_server_addr;
    tcp_server_addr.sin_family = AF_INET;

    //Serv ip:18.225.56.177
    //Local ip:127.0.0.1
    tcp_server_addr.sin_addr.s_addr = inet_addr("18.225.56.177");
    tcp_server_addr.sin_port = htons(8080);

    if (connect(tcp_socket, (struct sockaddr *)&tcp_server_addr, sizeof(tcp_server_addr)) < 0) {
        std::cerr << "Error connecting to TCP server" << std::endl;
        return 1;
    }

   // Setup UDP client
    int udp_socket = socket(AF_INET, SOCK_DGRAM, 0);
    sockaddr_in udp_client_addr;
    udp_client_addr.sin_family = AF_INET;
    udp_client_addr.sin_addr.s_addr = inet_addr(udp_ip);
    udp_client_addr.sin_port = htons(udp_port);

    bind(udp_socket, (struct sockaddr *)&udp_client_addr, sizeof(udp_client_addr));


    sockaddr_in udp_server_addr;
    udp_server_addr.sin_family = AF_INET;
    udp_server_addr.sin_addr.s_addr = inet_addr("18.225.56.177");
    udp_server_addr.sin_port = htons(8081);

    // Send autologin message over TCP
    std::string tcp_message = "1";
    if (send(tcp_socket, tcp_message.c_str(), tcp_message.size(), 0) < 0) {
        std::cerr << "Error sending TCP message" << std::endl;
        return 1;
    }

    // Send autologin message over UDP
    std::string udp_message = "1";
    if (sendto(udp_socket, udp_message.c_str(), udp_message.size(), 0, (struct sockaddr *)&udp_server_addr, sizeof(udp_server_addr)) < 0) {
        std::cerr << "Error sending UDP message" << std::endl;
        return 1;
    }

    std::thread tcp_thread(handle_tcp_connection, tcp_socket);
    std::thread udp_thread(handle_udp_connection, udp_socket);

    tcp_thread.join();
    udp_thread.join();

    close(tcp_socket);
    close(udp_socket);

    return 0;
}

void handle_tcp_connection(int tcp_socket) {
    char buffer[1024];
    while (true) {
        int bytes_received = read(tcp_socket, buffer, sizeof(buffer));
        if (bytes_received > 0) {
            buffer[bytes_received] = '\0';
            std::string message(buffer);
            std::istringstream ss(message.substr(1));
            std::string token;
            switch (message[0]) {
                case '1': {
                    client_config.nombres.clear();
                    while (std::getline(ss, token, ',')) {
                        client_config.nombres.push_back(token);
                    }
                    std::cout << "Updated names: ";
                    for (const auto& name : client_config.nombres) {
                        std::cout << name << " ";
                    }
                    std::cout << std::endl;
                    break;
                }
                case '2': {
                    client_config.ips.clear();
                    while (std::getline(ss, token, ',')) {
                        try {
                            client_config.ips.push_back(std::stoi(token));
                        } catch (const std::invalid_argument &e) {
                            std::cerr << "Invalid IP token: " << token << std::endl;
                        }
                    }
                    std::cout << "Updated IPs: ";
                    for (const auto& ip : client_config.ips) {
                        std::cout << ip << " ";
                    }
                    std::cout << std::endl;
                    break;
                }
                case '3': {
                    client_config.elegido.clear();
                    while (std::getline(ss, token, ',')) {
                        client_config.elegido.push_back(token == "1");
                    }
                    std::cout << "Updated selected: ";
                    for (const auto& selected : client_config.elegido) {
                        std::cout << selected << " ";
                    }
                    std::cout << std::endl;
                    break;
                }
                case 'H': {
                    if (!keep_alive_active) {
                        keep_alive_active = true;
                        std::thread(send_keep_alive, tcp_socket).detach();
                    }
                    break;
                }
                case 'G': { // Iniciar el entrenamiento desde el Terminal
                    processDataAprendisaje(DataAprendisaje, boards, nextMoves, target);
                    std::vector<double> sftmxacumulado = {0,0,0,0,0,0,0,0,0};
                    // calcular resultado
                    std::cout << "Generacion " << ++generacion << std::endl;
                    for (unsigned i = 0; i < boards.size(); ++i) {
                        std::vector<double> result;
                        // cout<<"Numero: "<<nextMoves[i]<<endl;
                        MLP.feedForward(boards[i]);
                        MLP.getResults(result); // vector<double> result | resultado del entrenamiento
                        sftmxResult = MLP.softmax(result); // cambiar el result con el softmax | sftmx es global
                        // enviar sftmx y numero de indice (unsigned i del for) a las demas RNs
                        //cout<<" board: "<<i <<endl;
                        for(int i = 0; i <sftmxResult.size();i++)
                        {
                            //cout<<"sftmxResul: "<<sftmxResult[i]<<endl;
                            sftmxacumulado[i] += sftmxResult[i];
                        }

                    }

                    string tcp_message = "m";
                    for(int i = 0; i <sftmxacumulado.size();i++)
                        {
                            sftmxacumulado[i]=sftmxacumulado[i]/boards.size();
                            sftmxacumulado[i]*=1000000;
                            //cout<<"sftmxacumulado por 1000000: "<<sftmxacumulado[i]*1000000<<endl;
                            //cout<<"sftmxacumulado: "<<sftmxacumulado[i]<<endl;
                            tcp_message+=to_string(sftmxacumulado[i])[0];
                            tcp_message+=to_string(sftmxacumulado[i])[1];
                            tcp_message+=to_string(sftmxacumulado[i])[2];
                            tcp_message+=to_string(sftmxacumulado[i])[3];
                            tcp_message+=to_string(sftmxacumulado[i])[4];
                            tcp_message+=to_string(sftmxacumulado[i])[5];
                            
                            
                        }
                    cout<<"OutPut RN: "<<tcp_message<<endl;
                    send(tcp_socket, tcp_message.c_str(), tcp_message.size(), 0);

                        
                    
                    break;
                }
                case 'C': { // Continuar el entrenamiento desde el Terminal
                    int generacion = 0;
                    while (generacion < 100) {
                        // calcular resultado
                        std::cout << "Generacion " << ++generacion << std::endl;
                        for (unsigned i = 0; i < boards.size(); ++i) {
                            std::vector<double> result;
                            // cout<<"Numero: "<<nextMoves[i]<<endl;
                            MLP.feedForward(boards[i]);
                            MLP.getResults(result); // vector<double> result | resultado del entrenamiento
                            sftmxResult = MLP.softmax(result); // cambiar el result con el softmaxResult | sftmxResult es global
                            // enviar sftmx y numero de indice (unsigned i del for) a las demas RNs
                        }
                    }
                    break;
                }
                case 'T': { // Recibir el tablero de la partida | Ej:T111111111000000000000000000

                    // Juego | T___111111111000000000000000000_
                    // T - idJugador de 3 bytes - tableroParsed - ficha
                    std::string idCliente = message.substr(1, 3); 
                    std::string tablero = message.substr(4, 27);;
                    int bestMove = 0;
                    // cout<<"Tablero:"<< tablero;
                    std::vector<double> board = stringToVector(tablero);
                    MLP.feedForward(board);
                    MLP.getResults(bestPlay);
                    sftmxBestPlay = MLP.softmax(bestPlay);

                    // showVector(" Resultado: ",result);
                    // showVector(" SoftMax: ", sftmx);

                    std::vector<int> bestPlayIndex(9);
                    for (int i = 0; i < 9; ++i) {
                        bestPlayIndex[i] = i;
                    }

                    std::sort(bestPlayIndex.begin(), bestPlayIndex.end(), compare_indices);

                    for (int play = 0; play < 9; ++play) {
                        if (tablero[bestPlayIndex[play]] != '0') {
                            bestMove = bestPlayIndex[play];
                            break;
                        }
                    }
                    std::cout << "Client's ID: " << idCliente << std::endl;
                    std::cout << "Best move: " << bestMove << std::endl;

                    // enviar al Main la posicion
                    string parsed = "T" + idCliente + to_string(bestMove);
                    write(tcp_socket, parsed.c_str(), parsed.length());
                    // T - idJugador - posicion
                    break;
                }
                case 'p': { // Recibir la media del resultado de cada entrenamiento
                    // recibir sftmxResult y numero de indice
                    // actualizar pesos
                    cout<<"MEDIA RECIBIDA: "<<message<<endl;
                    int i=0;
                    string value;
                    int count_interno = 0;
                    std::vector<double> media_recibida= {0,0,0,0,0,0,0,0,0};
                    for(int j =1;j<message.size();j++)
                    {
                        value+=message[j];
                        if(value.size()==6)
                        {
                            media_recibida[count_interno]+=stoi(value);
                            count_interno+=1;
                            value.clear();
                            //cout<<"pos vector: "<<count_interno-1<<endl;
                            //cout<<"media acumulada recibida "<<media_recibida[count_interno-1]<<endl;
                        }
                    }

                    for(int j = 0; j <media_recibida.size()-1;j++)
                    {
                        if(media_recibida[j] >= media_recibida[i])
                        {
                            i = j;
                            //cout<<"mayor: "<<media_recibida[i]<<endl;
                        }
                    }
                    //cout<<"posicion con mayor porcentaje de la media: "<<i<<endl;


                    if (MLP.checkError2(sftmxResult, target[nextMoves[i]])) {
                        MLP.backProp(target[nextMoves[i]]);
                    }
                    // showVector(" Resultado: ", sftmx);
                    // showVector("  Esperado: ", target[nextMoves[i]]);
                    // cin.ignore();
                    MLP.saveWeights();
                    std::cout << "Trained!" << std::endl;
                    break;
                }
                case 'm': { // Recibir la media de la mejor jugada
                    // Calculo de la media y enviar al main que enviara al cliente
                    count_output_rn_recibido+=1;
                    //cout<<"MENSAJE RECIBIDO PARA CALCULAR LA MEDIA: "<<message<<endl;
                    //cout<<"size mensaje: "<<message.size()<<endl;
                    string value;
                    int count_interno = 0;
                    for(int i =1;i<message.size();i++)
                    {
                        if(count_interno == 9)
                        {
                            count_output_rn_recibido+=1;
                            count_interno=0;
                            continue;

                        }
                        value+=message[i];
                        if(value.size()==6)
                        {
                            media_calculada[count_interno]+=stoi(value);
                            count_interno+=1;
                            value.clear();
                            //cout<<"pos vector: "<<count_interno-1<<endl;
                            //cout<<"media acumulada "<<media_calculada[count_interno-1]<<endl;
                        }
                    }

                    if(count_output_rn_recibido == client_config.ips.size())
                    {
                        string tcp_message = "p";
                        for(int i = 0;i <media_calculada.size();i++)
                        {
                            media_calculada[i]/=client_config.ips.size();
                            //cout<<"MEDIA CALCULADA: "<<media_calculada[i]<<endl;
                            tcp_message+=to_string(media_calculada[i])[0];
                            tcp_message+=to_string(media_calculada[i])[1];
                            tcp_message+=to_string(media_calculada[i])[2];
                            tcp_message+=to_string(media_calculada[i])[3];
                            tcp_message+=to_string(media_calculada[i])[4];
                            tcp_message+=to_string(media_calculada[i])[5];
                        }

                        send(tcp_socket, tcp_message.c_str(), tcp_message.size(), 0);
                        count_output_rn_recibido = 0;
                    }
                    

                    
                    break;
                }
                default:
                    std::cout << "Received TCP message: " << message << std::endl;
                    break;
            }
        } else {
            std::cerr << "Error receiving TCP message" << std::endl;
            break;
        }
    }
}

void handle_udp_connection(int udp_socket) {
    while (true) {
        sockaddr_in client_addr;
        socklen_t client_addr_len = sizeof(client_addr);
        recibirMensajesUDP packet;
        int bytes_received = recvfrom(udp_socket, &packet, sizeof(packet), 0, (struct sockaddr *)&client_addr, &client_addr_len);
        if (bytes_received > 0) {
            std::string client_ip = inet_ntoa(client_addr.sin_addr);
            {
                std::lock_guard<std::mutex> lock(data_mutex);
                client_udp_sockets[ntohs(client_addr.sin_port)] = client_addr;
            }
            process_udp_packet(packet, client_ip);
        } else {
            std::cerr << "Error receiving UDP message" << std::endl;
        }
    }
}

void process_udp_packet(const recibirMensajesUDP &packet, const std::string &client_ip) {
    uint32_t seq_num;
    {
        std::lock_guard<std::mutex> lock(seq_num_mutex);
        seq_num = expected_seq_num[client_ip];
    }

    uint16_t calculated_checksum = calcularChecksum(packet.dataMensaje, sizeof(packet.dataMensaje));
    if (packet.checksum != calculated_checksum) {
        std::cerr << "Checksum mismatch for client " << client_ip << std::endl;
        return;
    }

    std::istringstream ss(packet.dataMensaje);
    std::string token;
    switch (packet.Comando[0]) {
        case 'K': {
            std::vector<int> received_keys;
            while (std::getline(ss, token, ',')) {
                try {
                    received_keys.push_back(std::stoi(token));
                } catch (const std::invalid_argument &) {
                    std::cerr << "Invalid key token: " << token << std::endl;
                }
            }
            {
                std::lock_guard<std::mutex> lock(data_mutex);
                for (size_t i = 0; i < received_keys.size(); ++i) {
                    DataAprendisaje.insert({received_keys[i], ""});
                }
            }
            break;
        }
        case 'L': {
            std::vector<std::string> received_values;
            while (std::getline(ss, token, ',')) {
                received_values.push_back(token);
            }
            {
                std::lock_guard<std::mutex> lock(data_mutex);
                auto it = DataAprendisaje.begin();
                for (const auto& value : received_values) {
                    if (it != DataAprendisaje.end()) {
                        it->second = value;
                        ++it;
                    }
                }
            }
            break;
        }
        case 'Z': {
            std::lock_guard<std::mutex> lock(data_mutex);
            for (auto it = DataAprendisaje.begin(); it != DataAprendisaje.end(); ) {
                if (it->second.empty() || it->second == "L") {
                    it = DataAprendisaje.erase(it);
                } else {
                    ++it;
                }
            }
            break;
        }
        case 'D': {
            std::lock_guard<std::mutex> lock(data_mutex);
            print_data_aprendisaje();
            break;
        }
        default:
            std::cerr << "Invalid command: " << packet.Comando[0] << std::endl;
            return;
    }

    {
        std::lock_guard<std::mutex> lock(seq_num_mutex);
        expected_seq_num[client_ip]++;
    }
}


void send_keep_alive(int tcp_socket) {
    while (true) {
        auto now = std::chrono::system_clock::now();
        std::time_t now_c = std::chrono::system_clock::to_time_t(now);
        std::tm* now_tm = std::localtime(&now_c);
        
        std::stringstream ss;
        ss << "H:" << std::setw(2) << std::setfill('0') << now_tm->tm_hour << ":"
           << std::setw(2) << std::setfill('0') << now_tm->tm_min;
        
        std::string time_message = ss.str();
        if (send(tcp_socket, time_message.c_str(), time_message.size(), 0) < 0) {
            std::cerr << "Error sending keep-alive message" << std::endl;
        } else {
            std::cout << "Sent keep-alive message: " << time_message << std::endl; // Imprimir el mensaje enviado
        }
        std::this_thread::sleep_for(std::chrono::minutes(1));
    }
}

void print_client_config() {
    std::cout << "ClientConfig {" << std::endl;
    std::cout << "  nombres: [";
    for (size_t i = 0; i < client_config.nombres.size(); ++i) {
        std::cout << client_config.nombres[i];
        if (i < client_config.nombres.size() - 1) {
            std::cout << ", ";
        }
    }
    std::cout << "]," << std::endl;

    std::cout << "  ips: [";
    for (size_t i = 0; i < client_config.ips.size(); ++i) {
        std::cout << client_config.ips[i];
        if (i < client_config.ips.size() - 1) {
            std::cout << ", ";
        }
    }
    std::cout << "]," << std::endl;

    std::cout << "  elegido: [";
    for (size_t i = 0; i < client_config.elegido.size(); ++i) {
        std::cout << (client_config.elegido[i] ? "true" : "false");
        if (i < client_config.elegido.size() - 1) {
            std::cout << ", ";
        }
    }
    std::cout << "]" << std::endl;
    std::cout << "}" << std::endl;
}

void print_data_aprendisaje() {
    std::cout << "DataAprendisaje:" << std::endl;
    for (const auto& [key, value] : DataAprendisaje) {
        std::cout << "  " << key << " => " << value << std::endl;
    }
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
