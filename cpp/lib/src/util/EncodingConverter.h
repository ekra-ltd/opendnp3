#pragma once

#include "CharacterSet.h"

#include <string>

namespace opendnp3 {

    class EncodingConverter {
    public:
        static std::string FromSystem(const std::string& str, CharacterSet to);

        static std::string ToSystem(const std::string& str, CharacterSet from);

        static const std::string& GetSystemEncoding();
    };

}