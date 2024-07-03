#pragma once

#include "opendnp3/channel/IPEndpoint.h"


namespace opendnp3
{

/**
 *  Unsolicited response number of retries
 */
class ConnectionWithBackupEndpointConfig final
{
public:
    ConnectionWithBackupEndpointConfig(IPEndpoint main, IPEndpoint backup);

private:
    IPEndpoint _main;
    IPEndpoint _backup;

};

} // namespace opendnp3

