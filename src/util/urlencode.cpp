#include "urlencode.h"

#include <sstream>
#include <iomanip>

std::string util::urlEncode(const std::string &value)
{
    std::ostringstream escaped;
    escaped.fill('0');
    escaped << std::hex;

    for (char c : value) {
        if (std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
            escaped << c;
        } else {
            escaped << std::uppercase;
            escaped << '%' << std::setw(2) << int((unsigned char)c);
            escaped << std::nouppercase;
        }
    }

    return escaped.str();
}

std::string util::urlDecode(const std::string &value)
{
    std::string decoded;
    for (size_t i = 0; i < value.length(); ++i) {
        if (value[i] == '%') {
            if (i + 2 < value.length()) {
                std::string hex = value.substr(i + 1, 2);
                char decodedChar = static_cast<char>(std::stoi(hex, nullptr, 16));
                decoded += decodedChar;
                i += 2;
            }
        } else {
            decoded += value[i];
        }
    }
    return decoded;
}