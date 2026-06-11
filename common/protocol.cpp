#include "protocol.h"

#include <algorithm>
#include <cctype>
#include <sstream>

namespace protocol {

std::vector<std::string> split(const std::string& text, char sep) {
    std::vector<std::string> parts;
    std::string current;

    for (char c : text) {
        if (c == sep) {
            parts.push_back(current);
            current.clear();
        } else {
            current.push_back(c);
        }
    }

    parts.push_back(current);
    return parts;
}

MessageType getMessageType(const std::string& datagram) {
    const auto parts = split(datagram, '|');
    if (parts.empty()) return MessageType::UNKNOWN;

    if (parts[0] == "REG") return MessageType::REG;
    if (parts[0] == "REG_OK") return MessageType::REG_OK;
    if (parts[0] == "REG_ERR") return MessageType::REG_ERR;
    if (parts[0] == "MSG") return MessageType::MSG;
    if (parts[0] == "ACK") return MessageType::ACK;
    if (parts[0] == "LIST") return MessageType::LIST;
    if (parts[0] == "LIST_OK") return MessageType::LIST_OK;
    if (parts[0] == "QUIT") return MessageType::QUIT;
    if (parts[0] == "QUIT_OK") return MessageType::QUIT_OK;

    return MessageType::UNKNOWN;
}

bool isValidUsername(const std::string& username) {
    if (username.empty() || username.size() > MAX_USERNAME_SIZE) return false;

    for (char c : username) {
        const unsigned char uc = static_cast<unsigned char>(c);
        if (!(std::isalnum(uc) || c == '_' || c == '-')) {
            return false;
        }
    }

    return true;
}

std::string makeReg(const std::string& username, int recvPort) {
    return "REG|" + username + "|" + std::to_string(recvPort);
}

bool parseReg(const std::string& datagram, std::string& username, int& recvPort) {
    const auto parts = split(datagram, '|');
    if (parts.size() != 3 || parts[0] != "REG") return false;

    username = parts[1];

    try {
        recvPort = std::stoi(parts[2]);
    } catch (...) {
        return false;
    }

    return isValidUsername(username) && recvPort > 0 && recvPort <= 65535;
}

std::string makeRegOk(const std::string& username) {
    return "REG_OK|" + username;
}

std::string makeRegErr(const std::string& reason) {
    return "REG_ERR|" + reason;
}

std::string makeMsg(int msgId, const std::string& username, const std::string& text) {
    std::string cleanText = text;
    std::replace(cleanText.begin(), cleanText.end(), '\n', ' ');
    std::replace(cleanText.begin(), cleanText.end(), '\r', ' ');

    return "MSG|" + std::to_string(msgId) + "|" + username + "|" + cleanText;
}

bool parseMsg(const std::string& datagram, int& msgId, std::string& username, std::string& text) {
    const auto parts = split(datagram, '|');
    if (parts.size() < 4 || parts[0] != "MSG") return false;

    try {
        msgId = std::stoi(parts[1]);
    } catch (...) {
        return false;
    }

    username = parts[2];
    if (!isValidUsername(username)) return false;

    // El texto puede contener '|', así que reconstruimos desde parts[3].
    std::ostringstream oss;
    for (size_t i = 3; i < parts.size(); ++i) {
        if (i > 3) oss << "|";
        oss << parts[i];
    }
    text = oss.str();

    return msgId >= 0;
}

std::string makeAck(int msgId) {
    return "ACK|" + std::to_string(msgId);
}

bool parseAck(const std::string& datagram, int& msgId) {
    const auto parts = split(datagram, '|');
    if (parts.size() != 2 || parts[0] != "ACK") return false;

    try {
        msgId = std::stoi(parts[1]);
    } catch (...) {
        return false;
    }

    return msgId >= 0;
}

std::string makeList(const std::string& username) {
    return "LIST|" + username;
}

bool parseList(const std::string& datagram, std::string& username) {
    const auto parts = split(datagram, '|');
    if (parts.size() != 2 || parts[0] != "LIST") return false;
    username = parts[1];
    return isValidUsername(username);
}

std::string makeListOk(const std::vector<std::string>& usernames) {
    std::ostringstream oss;
    oss << "LIST_OK|";
    for (size_t i = 0; i < usernames.size(); ++i) {
        if (i > 0) oss << ",";
        oss << usernames[i];
    }
    return oss.str();
}

bool parseListOk(const std::string& datagram, std::vector<std::string>& usernames) {
    const auto parts = split(datagram, '|');
    if (parts.size() != 2 || parts[0] != "LIST_OK") return false;

    usernames.clear();
    if (parts[1].empty()) return true;

    const auto names = split(parts[1], ',');
    for (const auto& name : names) {
        if (!isValidUsername(name)) return false;
        usernames.push_back(name);
    }

    return true;
}

std::string makeQuit(const std::string& username) {
    return "QUIT|" + username;
}

bool parseQuit(const std::string& datagram, std::string& username) {
    const auto parts = split(datagram, '|');
    if (parts.size() != 2 || parts[0] != "QUIT") return false;
    username = parts[1];
    return isValidUsername(username);
}

std::string makeQuitOk(const std::string& username) {
    return "QUIT_OK|" + username;
}

bool parseQuitOk(const std::string& datagram, std::string& username) {
    const auto parts = split(datagram, '|');
    if (parts.size() != 2 || parts[0] != "QUIT_OK") return false;
    username = parts[1];
    return isValidUsername(username);
}

} // namespace protocol
