#define WIN32_LEAN_AND_MEAN

#include <winsock2.h>
#include <ws2tcpip.h>

#include <atomic>
#include <iostream>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "../common/protocol.h"

#pragma comment(lib, "Ws2_32.lib")

static std::mutex coutMutex;

static bool initWinsock() {
    WSADATA wsaData{};
    const int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
    return result == 0;
}

static SOCKET createBoundUdpSocket(unsigned short port) {
    SOCKET sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock == INVALID_SOCKET) {
        std::cerr << "[ERROR] socket() fallo: " << WSAGetLastError() << "\n";
        return INVALID_SOCKET;
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(port);

    if (bind(sock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == SOCKET_ERROR) {
        std::cerr << "[ERROR] bind(" << port << ") fallo: " << WSAGetLastError() << "\n";
        closesocket(sock);
        return INVALID_SOCKET;
    }

    return sock;
}

static sockaddr_in makeServerAddr(const std::string& ip, unsigned short port) {
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);

    if (inet_pton(AF_INET, ip.c_str(), &addr.sin_addr) != 1) {
        std::cerr << "[ERROR] IP de servidor invalida: " << ip << "\n";
    }

    return addr;
}

static void sendDatagram(SOCKET sock, const std::string& datagram, const sockaddr_in& dest) {
    const int sent = sendto(
        sock,
        datagram.c_str(),
        static_cast<int>(datagram.size()),
        0,
        reinterpret_cast<const sockaddr*>(&dest),
        sizeof(dest)
    );

    if (sent == SOCKET_ERROR) {
        std::lock_guard<std::mutex> lock(coutMutex);
        std::cerr << "[ERROR] sendto() fallo: " << WSAGetLastError() << "\n";
    }
}

static bool waitForDatagram(SOCKET sock, int timeoutMs, std::string& datagram) {
    fd_set readSet;
    FD_ZERO(&readSet);
    FD_SET(sock, &readSet);

    timeval timeout{};
    timeout.tv_sec = timeoutMs / 1000;
    timeout.tv_usec = (timeoutMs % 1000) * 1000;

    const int ready = select(0, &readSet, nullptr, nullptr, &timeout);

    if (ready == SOCKET_ERROR) {
        std::lock_guard<std::mutex> lock(coutMutex);
        std::cerr << "[ERROR] select() fallo: " << WSAGetLastError() << "\n";
        return false;
    }

    if (ready == 0) {
        return false;
    }

    char buffer[protocol::MAX_DATAGRAM_SIZE + 1]{};
    sockaddr_in from{};
    int fromLen = sizeof(from);

    const int received = recvfrom(
        sock,
        buffer,
        protocol::MAX_DATAGRAM_SIZE,
        0,
        reinterpret_cast<sockaddr*>(&from),
        &fromLen
    );

    if (received == SOCKET_ERROR) {
        std::lock_guard<std::mutex> lock(coutMutex);
        std::cerr << "[ERROR] recvfrom() fallo: " << WSAGetLastError() << "\n";
        return false;
    }

    buffer[received] = '\0';
    datagram.assign(buffer, received);
    return true;
}

static bool registerInServer(
    SOCKET sendSocket,
    const std::string& username,
    unsigned short recvPort,
    const sockaddr_in& regAddr
) {
    const std::string reg = protocol::makeReg(username, recvPort);

    {
        std::lock_guard<std::mutex> lock(coutMutex);
        std::cout << "[REG TX] " << reg << "\n";
    }

    sendDatagram(sendSocket, reg, regAddr);

    std::string response;
    if (!waitForDatagram(sendSocket, protocol::ACK_TIMEOUT_MS, response)) {
        std::lock_guard<std::mutex> lock(coutMutex);
        std::cerr << "[ERROR] No se recibio respuesta de registro.\n";
        return false;
    }

    {
        std::lock_guard<std::mutex> lock(coutMutex);
        std::cout << "[REG RX] " << response << "\n";
    }

    return protocol::getMessageType(response) == protocol::MessageType::REG_OK;
}

static bool waitForAck(SOCKET sendSocket, int expectedMsgId) {
    std::string response;

    if (!waitForDatagram(sendSocket, protocol::ACK_TIMEOUT_MS, response)) {
        return false;
    }

    int receivedMsgId = -1;
    if (!protocol::parseAck(response, receivedMsgId)) {
        std::lock_guard<std::mutex> lock(coutMutex);
        std::cerr << "[WARN] Respuesta inesperada mientras se esperaba ACK: " << response << "\n";
        return false;
    }

    return receivedMsgId == expectedMsgId;
}

static void receiverLoop(SOCKET recvSocket, std::atomic<bool>& running) {
    while (running.load()) {
        std::string datagram;

        if (!waitForDatagram(recvSocket, 300, datagram)) {
            continue;
        }

        int msgId = -1;
        std::string username;
        std::string text;

        std::lock_guard<std::mutex> lock(coutMutex);

        if (protocol::parseMsg(datagram, msgId, username, text)) {
            std::cout << "\n[" << username << " #" << msgId << "] " << text << "\n> ";
        } else {
            std::cout << "\n[RX] " << datagram << "\n> ";
        }

        std::cout.flush();
    }
}

static void requestUserList(SOCKET sendSocket, const std::string& username, const sockaddr_in& dataAddr) {
    const std::string request = protocol::makeList(username);
    sendDatagram(sendSocket, request, dataAddr);

    std::string response;
    if (!waitForDatagram(sendSocket, protocol::ACK_TIMEOUT_MS, response)) {
        std::lock_guard<std::mutex> lock(coutMutex);
        std::cout << "[LIST] No se recibio respuesta del servidor.\n";
        return;
    }

    std::vector<std::string> users;
    if (!protocol::parseListOk(response, users)) {
        std::lock_guard<std::mutex> lock(coutMutex);
        std::cout << "[LIST] Respuesta inesperada: " << response << "\n";
        return;
    }

    std::lock_guard<std::mutex> lock(coutMutex);
    std::cout << "[LIST] Usuarios conectados (" << users.size() << "): ";
    if (users.empty()) {
        std::cout << "<ninguno>";
    }
    for (size_t i = 0; i < users.size(); ++i) {
        if (i > 0) std::cout << ", ";
        std::cout << users[i];
    }
    std::cout << "\n";
}

static void sendQuit(SOCKET sendSocket, const std::string& username, const sockaddr_in& dataAddr) {
    const std::string request = protocol::makeQuit(username);
    sendDatagram(sendSocket, request, dataAddr);

    std::string response;
    if (waitForDatagram(sendSocket, 500, response)) {
        std::string confirmedUsername;
        if (protocol::parseQuitOk(response, confirmedUsername)) {
            std::lock_guard<std::mutex> lock(coutMutex);
            std::cout << "[QUIT] Servidor confirmo salida de " << confirmedUsername << "\n";
        }
    }
}

static void printUsage(const char* exeName) {
    std::cout << "Uso:\n";
    std::cout << "  " << exeName << " <usuario> <server_ip> <reg_port> <data_port> <base_port>\n\n";
    std::cout << "Ejemplo:\n";
    std::cout << "  " << exeName << " Pablo 127.0.0.1 40000 40001 50000\n\n";
    std::cout << "El cliente usara:\n";
    std::cout << "  socket envio     -> base_port\n";
    std::cout << "  socket recepcion -> base_port + 1\n";
}

int main(int argc, char* argv[]) {
    if (argc < 6) {
        printUsage(argv[0]);
        return 1;
    }

    const std::string username = argv[1];
    const std::string serverIp = argv[2];
    const unsigned short regPort = static_cast<unsigned short>(std::stoi(argv[3]));
    const unsigned short dataPort = static_cast<unsigned short>(std::stoi(argv[4]));
    const unsigned short basePort = static_cast<unsigned short>(std::stoi(argv[5]));
    const unsigned short recvPort = static_cast<unsigned short>(basePort + 1);

    if (!protocol::isValidUsername(username)) {
        std::cerr << "[ERROR] Nombre de usuario invalido. Usa letras, numeros, '_' o '-'.\n";
        return 1;
    }

    if (!initWinsock()) {
        std::cerr << "[ERROR] No se pudo inicializar Winsock.\n";
        return 1;
    }

    SOCKET sendSocket = createBoundUdpSocket(basePort);
    SOCKET recvSocket = createBoundUdpSocket(recvPort);

    if (sendSocket == INVALID_SOCKET || recvSocket == INVALID_SOCKET) {
        WSACleanup();
        return 1;
    }

    const sockaddr_in regAddr = makeServerAddr(serverIp, regPort);
    const sockaddr_in dataAddr = makeServerAddr(serverIp, dataPort);

    std::cout << "=== Cliente APL05 ===\n";
    std::cout << "Usuario: " << username << "\n";
    std::cout << "Servidor: " << serverIp << "\n";
    std::cout << "Puerto envio: " << basePort << "\n";
    std::cout << "Puerto recepcion: " << recvPort << "\n\n";

    if (!registerInServer(sendSocket, username, recvPort, regAddr)) {
        std::cerr << "[ERROR] Registro fallido.\n";
        closesocket(sendSocket);
        closesocket(recvSocket);
        WSACleanup();
        return 1;
    }

    std::atomic<bool> running{true};
    std::thread receiver(receiverLoop, recvSocket, std::ref(running));

    std::cout << "\nRegistro correcto. Escribe mensajes y pulsa ENTER.\n";
    std::cout << "Comandos disponibles:\n";
    std::cout << "  /list  muestra usuarios registrados\n";
    std::cout << "  /quit  sale y elimina el usuario de la tabla del servidor\n\n";

    int nextMsgId = 1;
    std::string line;

    while (running.load()) {
        {
            std::lock_guard<std::mutex> lock(coutMutex);
            std::cout << "> ";
            std::cout.flush();
        }

        if (!std::getline(std::cin, line)) {
            break;
        }

        if (line == "/quit") {
            sendQuit(sendSocket, username, dataAddr);
            running.store(false);
            break;
        }

        if (line == "/list") {
            requestUserList(sendSocket, username, dataAddr);
            continue;
        }

        if (line.empty()) {
            continue;
        }

        const int msgId = nextMsgId++;
        const std::string msg = protocol::makeMsg(msgId, username, line);

        bool ackReceived = false;

        for (int attempt = 1; attempt <= protocol::MAX_RETRIES; ++attempt) {
            {
                std::lock_guard<std::mutex> lock(coutMutex);
                std::cout << "[MSG TX] intento " << attempt << ": " << msg << "\n";
            }

            sendDatagram(sendSocket, msg, dataAddr);

            if (waitForAck(sendSocket, msgId)) {
                std::lock_guard<std::mutex> lock(coutMutex);
                std::cout << "[ACK RX] ACK|" << msgId << "\n";
                ackReceived = true;
                break;
            }

            {
                std::lock_guard<std::mutex> lock(coutMutex);
                std::cout << "[TIMEOUT] No llego ACK para MSG " << msgId << "\n";
            }
        }

        if (!ackReceived) {
            std::lock_guard<std::mutex> lock(coutMutex);
            std::cerr << "[WARN] Mensaje sin confirmar tras "
                      << protocol::MAX_RETRIES << " intentos.\n";
        }
    }

    running.store(false);

    if (receiver.joinable()) {
        receiver.join();
    }

    closesocket(sendSocket);
    closesocket(recvSocket);
    WSACleanup();

    std::cout << "\nCliente finalizado.\n";
    return 0;
}
