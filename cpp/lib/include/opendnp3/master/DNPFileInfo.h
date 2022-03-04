#pragma once

#include "opendnp3/app/DNPTime.h"

#include <cstdint>
#include <string>

namespace opendnp3
{

enum class DNPFileType : uint16_t
{
    DIRECTORY = 0x0,
    SIMPLE_FILE = 0x1
};

enum class DNPFileOperationPermission : uint16_t
{
    OWNER_READ_ALLOWED = 0x0100,
    OWNER_WRITE_ALLOWED = 0x0080,
    OWNER_EXECUTE_ALLOWED = 0x0040,
    GROUP_READ_ALLOWED = 0x0020,
    GROUP_WRITE_ALLOWED = 0x0010,
    GROUP_EXECUTE_ALLOWED = 0x0008,
    WORLD_READ_ALLOWED = 0x0004,
    WORLD_WRITE_ALLOWED = 0x0002,
    WORLD_EXECUTE_ALLOWED = 0x0001,
    NONE = 0x0
};

inline DNPFileOperationPermission operator|(DNPFileOperationPermission a, DNPFileOperationPermission b)
{
    return static_cast<DNPFileOperationPermission>(static_cast<int>(a) | static_cast<int>(b));
}

inline void operator|=(uint16_t& a, DNPFileOperationPermission b) {
    a |= static_cast<uint16_t>(b);
}

inline void operator|=(DNPFileOperationPermission& a, DNPFileOperationPermission b) {
    a = a | b;
}

struct DNPFileInfo
{
    DNPFileType fileType{ DNPFileType::DIRECTORY };
    uint32_t fileSize{ 0 };
    DNPTime timeOfCreation{ 0 };
    DNPFileOperationPermission permissions;
    std::string filename;
};

} // namespace opendnp3