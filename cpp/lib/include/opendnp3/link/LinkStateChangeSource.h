#pragma once

#include <cstdint>

namespace opendnp3
{

    enum class LinkStateChangeSource : uint8_t
    {
        PrimaryChannel,
        BackupChannel,
        Unconditional,
        Ignore
    };

}
