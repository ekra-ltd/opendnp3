#pragma once

#include "link/LinkLayerConstants.h"

#include "opendnp3/StatisticsTypes.h"
#include "opendnp3/channel/IListener.h"
#include "opendnp3/channel/IPEndpoint.h"
#include "opendnp3/logging/Logger.h"
#include "opendnp3/util/Uncopyable.h"

#include <exe4cpp/asio/StrandExecutor.h>

#include <memory>

namespace opendnp3
{

class UDPChannelListener : public std::enable_shared_from_this<UDPChannelListener>, public IListener, private Uncopyable
{
public:
    UDPChannelListener(
        Logger& logger,
        std::shared_ptr<exe4cpp::StrandExecutor> executor
    );

    void Shutdown() final;

protected:
    void Bind(const IPEndpoint& localEndpoint, std::error_code& ec);
    virtual void OnSocketReady(asio::ip::udp::socket socket) = 0;

private:
    void StartReceive();
    void Finish();

private:
    Logger& logger;
    std::shared_ptr<exe4cpp::StrandExecutor> executor;
    asio::ip::udp::socket socket;
    asio::ip::udp::endpoint remote_endpoint;
    std::array<uint8_t, LPDU_MAX_FRAME_SIZE> buf;
    bool finished;
    bool isShutdown;
};

} // namespace opendnp3
