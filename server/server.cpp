#define WIN32_LEAN_AND_MEAN

#include <winsock2.h>
#include <ws2tcpip.h>

#include <algorithm>
#include <cstdlib>
#include <ctime>
#include <iostream>
#include <string>
#include <vector>

#include "../common/protocol.h"

#pragma comment(lib, "Ws2_32.lib")

struct User {
    std::string username;
    sockaddr_in recvAddr{};
    int lastMsgId = -1;  // ultimo msg_id difundido de este usuario (deduplicacion)
};

static bool initWinsock() {
    WSADATA wsaData{};
    const int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
    return result == 0;
}

static SOCKET createUdpSocket(unsigned short port) {
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

static std::string addrToString(const sockaddr_in& addr) {
    char ip[INET_ADDRSTRLEN]{};
    inet_ntop(AF_INET, const_cast<IN_ADDR*>(&addr.sin_addr), ip, sizeof(ip));

    return std::string(ip) + ":" + std::to_string(ntohs(addr.sin_port));
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
        std::cerr << "[ERROR] sendto() fallo: " << WSAGetLastError() << "\n";
    }
}

static bool receiveDatagram(SOCKET sock, std::string& datagram, sockaddr_in& from) {
    char buffer[protocol::MAX_DATAGRAM_SIZE + 1]{};
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
        std::cerr << "[ERROR] recvfrom() fallo: " << WSAGetLastError() << "\n";
        return false;
    }

    buffer[received] = '\0';
    datagram.assign(buffer, received);
    return true;
}

static void printUserTable(const std::vector<User>& users) {
    std::cout << "[USERS] " << users.size() << " usuario(s) registrado(s): ";
    if (users.empty()) {
        std::cout << "<ninguno>";
    }

    for (size_t i = 0; i < users.size(); ++i) {
        if (i > 0) std::cout << ", ";
        std::cout << users[i].username << "@" << addrToString(users[i].recvAddr);
    }

    std::cout << "\n";
}

static std::vector<std::string> getUsernames(const std::vector<User>& users) {
    std::vector<std::string> names;
    for (const User& user : users) {
        names.push_back(user.username);
    }
    return names;
}

// Difunde a todos los usuarios registrados un mensaje de sistema (remitente
// "Servidor", id 0). El cliente lo muestra como un mensaje normal mas.
static void broadcastSystemMessage(
    SOCKET dataSocket,
    const std::vector<User>& users,
    const std::string& text
) {
    const std::string datagram = protocol::makeMsg(0, "Servidor", text);
    for (const User& user : users) {
        sendDatagram(dataSocket, datagram, user.recvAddr);
    }
    std::cout << "[SYS TX] " << text << " (" << users.size() << " destinatarios)\n";
}

static bool registerUser(
    std::vector<User>& users,
    const std::string& username,
    const sockaddr_in& senderAddr,
    int recvPort
) {
    sockaddr_in recvAddr = senderAddr;
    recvAddr.sin_port = htons(static_cast<unsigned short>(recvPort));

    auto it = std::find_if(users.begin(), users.end(), [&](const User& u) {
        return u.username == username;
    });

    bool isNew = false;

    if (it == users.end()) {
        users.push_back(User{username, recvAddr});
        std::cout << "[REG] Usuario nuevo: " << username
                  << " recv=" << addrToString(recvAddr) << "\n";
        isNew = true;
    } else {
        it->recvAddr = recvAddr;
        it->lastMsgId = -1;  // nueva sesion del cliente: reiniciar deduplicacion
        std::cout << "[REG] Usuario actualizado: " << username
                  << " recv=" << addrToString(recvAddr) << "\n";
    }

    printUserTable(users);
    return isNew;
}

static bool removeUser(std::vector<User>& users, const std::string& username) {
    const size_t oldSize = users.size();

    users.erase(
        std::remove_if(users.begin(), users.end(), [&](const User& user) {
            return user.username == username;
        }),
        users.end()
    );

    return users.size() != oldSize;
}

static void handleRegistration(SOCKET regSocket, SOCKET dataSocket, std::vector<User>& users) {
    std::string datagram;
    sockaddr_in from{};

    if (!receiveDatagram(regSocket, datagram, from)) return;

    std::cout << "[REG RX] " << addrToString(from) << " -> " << datagram << "\n";

    std::string username;
    int recvPort = 0;

    if (!protocol::parseReg(datagram, username, recvPort)) {
        const std::string err = protocol::makeRegErr("Formato REG invalido");
        sendDatagram(regSocket, err, from);
        return;
    }

    const bool isNew = registerUser(users, username, from, recvPort);

    const std::string ok = protocol::makeRegOk(username);
    sendDatagram(regSocket, ok, from);

    if (isNew) {
        broadcastSystemMessage(dataSocket, users, username + " se ha unido al chat");
    }
}

static bool shouldDropAck(int dropAckPercent) {
    if (dropAckPercent <= 0) return false;
    if (dropAckPercent >= 100) return true;
    return (std::rand() % 100) < dropAckPercent;
}

static void handleChatMessage(
    SOCKET dataSocket,
    std::vector<User>& users,
    int dropAckPercent
) {
    std::string datagram;
    sockaddr_in from{};

    if (!receiveDatagram(dataSocket, datagram, from)) return;

    std::cout << "[DATA RX] " << addrToString(from) << " -> " << datagram << "\n";

    switch (protocol::getMessageType(datagram)) {
        case protocol::MessageType::MSG: {
            int msgId = -1;
            std::string username;
            std::string text;

            if (!protocol::parseMsg(datagram, msgId, username, text)) {
                std::cerr << "[WARN] Mensaje de chat invalido. Se ignora.\n";
                return;
            }

            // Deduplicacion: si el cliente reenvia el mismo msg_id porque se
            // perdio el ACK, no volvemos a difundir; basta con reenviar el ACK.
            auto sender = std::find_if(users.begin(), users.end(), [&](const User& u) {
                return u.username == username;
            });
            const bool isDuplicate = (sender != users.end() && msgId <= sender->lastMsgId);

            if (isDuplicate) {
                std::cout << "[DUP] MSG " << msgId << " de " << username
                          << " ya difundido; no se redifunde\n";
            } else {
                if (sender != users.end()) sender->lastMsgId = msgId;

                for (const User& user : users) {
                    sendDatagram(dataSocket, datagram, user.recvAddr);
                    std::cout << "[MSG TX] a " << user.username
                              << " (" << addrToString(user.recvAddr) << ")\n";
                }
            }

            const std::string ack = protocol::makeAck(msgId);

            if (shouldDropAck(dropAckPercent)) {
                std::cout << "[ACK DROP] Simulada perdida de " << ack
                          << " para " << addrToString(from) << "\n";
            } else {
                sendDatagram(dataSocket, ack, from);
                std::cout << "[ACK TX] " << ack << " a " << addrToString(from) << "\n";
            }
            break;
        }

        case protocol::MessageType::LIST: {
            std::string username;
            if (!protocol::parseList(datagram, username)) {
                std::cerr << "[WARN] LIST invalido. Se ignora.\n";
                return;
            }

            const std::string response = protocol::makeListOk(getUsernames(users));
            sendDatagram(dataSocket, response, from);
            std::cout << "[LIST TX] " << response << " a " << username << "\n";
            break;
        }

        case protocol::MessageType::QUIT: {
            std::string username;
            if (!protocol::parseQuit(datagram, username)) {
                std::cerr << "[WARN] QUIT invalido. Se ignora.\n";
                return;
            }

            const bool removed = removeUser(users, username);
            const std::string response = protocol::makeQuitOk(username);
            sendDatagram(dataSocket, response, from);

            std::cout << "[QUIT] " << username
                      << (removed ? " eliminado" : " no estaba registrado") << "\n";

            if (removed) {
                broadcastSystemMessage(dataSocket, users, username + " ha salido del chat");
            }
            printUserTable(users);
            break;
        }

        default:
            std::cerr << "[WARN] Tipo de mensaje no soportado en puerto de datos.\n";
            break;
    }
}

static void printUsage(const char* exeName) {
    std::cout << "Uso:\n";
    std::cout << "  " << exeName << " <reg_port> <data_port> [drop_ack_percent]\n\n";
    std::cout << "Ejemplos:\n";
    std::cout << "  " << exeName << " 40000 40001\n";
    std::cout << "  " << exeName << " 40000 40001 30\n\n";
    std::cout << "drop_ack_percent permite simular perdida de ACKs para probar reenvios.\n";
}

int main(int argc, char* argv[]) {
    unsigned short regPort = 40000;
    unsigned short dataPort = 40001;
    int dropAckPercent = 0;

    if (argc == 2 && std::string(argv[1]) == "--help") {
        printUsage(argv[0]);
        return 0;
    }

    if (argc >= 3) {
        regPort = static_cast<unsigned short>(std::stoi(argv[1]));
        dataPort = static_cast<unsigned short>(std::stoi(argv[2]));
    }

    if (argc >= 4) {
        dropAckPercent = std::stoi(argv[3]);
        if (dropAckPercent < 0) dropAckPercent = 0;
        if (dropAckPercent > 100) dropAckPercent = 100;
    }

    std::srand(static_cast<unsigned int>(std::time(nullptr)));

    if (!initWinsock()) {
        std::cerr << "[ERROR] No se pudo inicializar Winsock.\n";
        return 1;
    }

    SOCKET regSocket = createUdpSocket(regPort);
    SOCKET dataSocket = createUdpSocket(dataPort);

    if (regSocket == INVALID_SOCKET || dataSocket == INVALID_SOCKET) {
        WSACleanup();
        return 1;
    }

    std::vector<User> users;

    std::cout << "=== Servidor APL05 iniciado ===\n";
    std::cout << "Puerto registro: " << regPort << "\n";
    std::cout << "Puerto difusion: " << dataPort << "\n";
    std::cout << "Perdida simulada de ACKs: " << dropAckPercent << "%\n";
    std::cout << "Esperando clientes...\n\n";

    while (true) {
        fd_set readSet;
        FD_ZERO(&readSet);
        FD_SET(regSocket, &readSet);
        FD_SET(dataSocket, &readSet);

        timeval timeout{};
        timeout.tv_sec = 1;
        timeout.tv_usec = 0;

        const int ready = select(0, &readSet, nullptr, nullptr, &timeout);

        if (ready == SOCKET_ERROR) {
            std::cerr << "[ERROR] select() fallo: " << WSAGetLastError() << "\n";
            break;
        }

        if (ready == 0) {
            continue;
        }

        if (FD_ISSET(regSocket, &readSet)) {
            handleRegistration(regSocket, dataSocket, users);
        }

        if (FD_ISSET(dataSocket, &readSet)) {
            handleChatMessage(dataSocket, users, dropAckPercent);
        }
    }

    closesocket(regSocket);
    closesocket(dataSocket);
    WSACleanup();

    return 0;
}
