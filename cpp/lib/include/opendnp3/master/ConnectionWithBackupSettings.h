#pragma once

#include <opendnp3/channel/ChannelConnectionOptions.h>

namespace opendnp3
{

    struct ConnectionWithBackupSettings
    {
        ChannelConnectionOptions Primary;
        ChannelConnectionOptions Backup;
    };

} // namespace opendnp3
