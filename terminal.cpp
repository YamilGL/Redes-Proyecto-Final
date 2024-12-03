#include <iostream>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/socket.h>

int main() {
    // Setup TCP client
    int tcp_socket = socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in tcp_server_addr;
    tcp_server_addr.sin_family = AF_INET;
    tcp_server_addr.sin_addr.s_addr = inet_addr("18.225.56.177");
    tcp_server_addr.sin_port = htons(8080);

    connect(tcp_socket, (struct sockaddr *)&tcp_server_addr, sizeof(tcp_server_addr));

    std::string message;
    while (true) {
        std::cout << "G: Start training" << std::endl;
        std::cout << "C: Continue training" << std::endl;
        std::cout << "Enter message: " << std::endl;
        std::getline(std::cin, message);

        send(tcp_socket, message.c_str(), message.size(), 0);

        if (message == "exit") {
            break;
        }
    }

    close(tcp_socket);

    return 0;
}