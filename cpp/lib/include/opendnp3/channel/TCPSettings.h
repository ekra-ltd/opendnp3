#pragma once

#include "opendnp3/channel/IPEndpointsList.h"

#include <chrono>
#include <ostream>

namespace opendnp3
{

    struct TCPSettings final
    {
        IPEndpointsList Endpoints;

        bool ForceKeepAlive{ false }; // Force enable keep-alive packets send
        std::chrono::seconds KeepIdle{ 0 }; // Idle timeout in seconds before keep-alive packets send
        std::chrono::seconds KeepIntvl{ 0 }; // Keep-alive packets send interval in seconds

        friend bool operator==(const TCPSettings& lhs, const TCPSettings& rhs)
        {
            return lhs.Endpoints == rhs.Endpoints
                && lhs.ForceKeepAlive == rhs.ForceKeepAlive
                && lhs.KeepIdle == rhs.KeepIdle
                && lhs.KeepIntvl == rhs.KeepIntvl;
        }

        friend bool operator!=(const TCPSettings& lhs, const TCPSettings& rhs)
        {
            return !(lhs == rhs);
        }

        friend std::ostream& operator<<(std::ostream& os, const TCPSettings& obj)
        {
            return os
                << "Endpoints: " << obj.Endpoints
                << " ForceKeepAlive: " << obj.ForceKeepAlive
                << " KeepIdle: " << obj.KeepIdle.count()
                << " KeepIntvl: " << obj.KeepIntvl.count()
            ;
        }
    };

} // namespace opendnp3
