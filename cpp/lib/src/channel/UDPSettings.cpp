#include "opendnp3/channel/UDPSettings.h"

namespace opendnp3
{

    UDPSettings::UDPSettings(IPEndpoint local, IPEndpoint remote)
        : Local(std::move(local))
        , Remote(std::move(remote))
    {}

} // namespace opendnp3
