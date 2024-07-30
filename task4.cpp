#include <iostream>
#include <thread>
#include <vector>
#include <mutex>
#include <sstream>
#include <fstream>
#include <string>
#include <winsock2.h>
#include <ws2tcpip.h>

#pragma comment(lib, "Ws2_32.lib")

const int PORT = 8080;
const std::string HTML_DIR = "./html";

std::mutex cout_mutex;

void handle_client(SOCKET client_socket)
{
    char buffer[1024] = {0};
    int valread = recv(client_socket, buffer, 1024, 0);

    {
        std::lock_guard<std::mutex> lock(cout_mutex);
        std::cout << "Received request:\n"
                  << buffer << std::endl;
    }

    if (valread == SOCKET_ERROR)
    {
        std::cerr << "Failed to read from socket: " << WSAGetLastError() << std::endl;
        closesocket(client_socket);
        return;
    }

    std::istringstream request(buffer);
    std::string method, path, version;
    request >> method >> path >> version;

    if (path == "/")
    {
        path = "/index.html";
    }

    std::string file_path = HTML_DIR + path;
    std::ifstream file(file_path);
    std::string response;

    if (file.is_open())
    {
        std::stringstream file_stream;
        file_stream << file.rdbuf();
        std::string content = file_stream.str();

        response = "HTTP/1.1 200 OK\r\n";
        response += "Content-Type: text/html\r\n";
        response += "Content-Length: " + std::to_string(content.size()) + "\r\n";
        response += "\r\n";
        response += content;
    }
    else
    {
        std::string not_found = "<html><body><h1>404 Not Found</h1></body></html>";

        response = "HTTP/1.1 404 Not Found\r\n";
        response += "Content-Type: text/html\r\n";
        response += "Content-Length: " + std::to_string(not_found.size()) + "\r\n";
        response += "\r\n";
        response += not_found;
    }

    send(client_socket, response.c_str(), response.size(), 0);
    closesocket(client_socket);
}

int main()
{
    WSADATA wsaData;
    int iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (iResult != 0)
    {
        std::cerr << "WSAStartup failed: " << iResult << std::endl;
        return 1;
    }

    SOCKET server_fd = INVALID_SOCKET;
    SOCKET client_socket = INVALID_SOCKET;
    struct sockaddr_in address;
    int addrlen = sizeof(address);

    server_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (server_fd == INVALID_SOCKET)
    {
        std::cerr << "Socket creation failed: " << WSAGetLastError() << std::endl;
        WSACleanup();
        return 1;
    }

    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, (char *)&opt, sizeof(opt)) == SOCKET_ERROR)
    {
        std::cerr << "setsockopt failed: " << WSAGetLastError() << std::endl;
        closesocket(server_fd);
        WSACleanup();
        return 1;
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) == SOCKET_ERROR)
    {
        std::cerr << "Bind failed: " << WSAGetLastError() << std::endl;
        closesocket(server_fd);
        WSACleanup();
        return 1;
    }

    if (listen(server_fd, 10) == SOCKET_ERROR)
    {
        std::cerr << "Listen failed: " << WSAGetLastError() << std::endl;
        closesocket(server_fd);
        WSACleanup();
        return 1;
    }

    std::vector<std::thread> threads;

    while (true)
    {
        client_socket = accept(server_fd, (struct sockaddr *)&address, &addrlen);
        if (client_socket == INVALID_SOCKET)
        {
            std::cerr << "Accept failed: " << WSAGetLastError() << std::endl;
            continue;
        }

        {
            std::lock_guard<std::mutex> lock(cout_mutex);
            std::cout << "Accepted new connection" << std::endl;
        }

        threads.emplace_back(handle_client, client_socket);
    }

    for (auto &thread : threads)
    {
        if (thread.joinable())
        {
            thread.join();
        }
    }

    closesocket(server_fd);
    WSACleanup();
    return 0;
}
