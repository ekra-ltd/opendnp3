#pragma once

#include "opendnp3/channel/IPEndpoint.h"

#include <ostream>

namespace opendnp3
{

    struct UDPSettings final
    {
        UDPSettings(IPEndpoint local, IPEndpoint remote);

        IPEndpoint Local;
        IPEndpoint Remote;

        friend bool operator==(const UDPSettings& lhs, const UDPSettings& rhs)
        {
            return lhs.Local == rhs.Local
                && lhs.Remote == rhs.Remote;
        }

        friend bool operator!=(const UDPSettings& lhs, const UDPSettings& rhs)
        {
            return !(lhs == rhs);
        }

        friend std::ostream& operator<<(std::ostream& os, const UDPSettings& obj)
        {
            return os
                << "Local: " << obj.Local
                << " Remote: " << obj.Remote;
        }
    };

} // namespace opendnp3
