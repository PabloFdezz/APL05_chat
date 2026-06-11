#pragma once

#include <string>
#include <vector>

namespace protocol {

constexpr int MAX_DATAGRAM_SIZE = 1024;
constexpr int MAX_USERNAME_SIZE = 32;
constexpr int ACK_TIMEOUT_MS = 2000;
constexpr int MAX_RETRIES = 3;

enum class MessageType {
    REG,
    REG_OK,
    REG_ERR,
    MSG,
    ACK,
    LIST,
    LIST_OK,
    QUIT,
    QUIT_OK,
    UNKNOWN
};

std::vector<std::string> split(const std::string& text, char sep);
MessageType getMessageType(const std::string& datagram);

bool isValidUsername(const std::string& username);

std::string makeReg(const std::string& username, int recvPort);
bool parseReg(const std::string& datagram, std::string& username, int& recvPort);

std::string makeRegOk(const std::string& username);
std::string makeRegErr(const std::string& reason);

std::string makeMsg(int msgId, const std::string& username, const std::string& text);
bool parseMsg(const std::string& datagram, int& msgId, std::string& username, std::string& text);

std::string makeAck(int msgId);
bool parseAck(const std::string& datagram, int& msgId);

std::string makeList(const std::string& username);
bool parseList(const std::string& datagram, std::string& username);

std::string makeListOk(const std::vector<std::string>& usernames);
bool parseListOk(const std::string& datagram, std::vector<std::string>& usernames);

std::string makeQuit(const std::string& username);
bool parseQuit(const std::string& datagram, std::string& username);

std::string makeQuitOk(const std::string& username);
bool parseQuitOk(const std::string& datagram, std::string& username);

} // namespace protocol
