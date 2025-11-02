#pragma once

#include <string>

namespace util
{
    // URL-encode a string
    std::string urlEncode(const std::string& value);
    
    // URL-decode a string
    std::string urlDecode(const std::string& value);
} // namespace util