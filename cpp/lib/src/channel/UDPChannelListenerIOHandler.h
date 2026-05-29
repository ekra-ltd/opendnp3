#pragma once

#include "channel/IOHandler.h"

#include "opendnp3/channel/IPEndpointsList.h"
#include "opendnp3/gen/ServerAcceptMode.h"

#include <utility>

namespace opendnp3
{

class UDPServerSocketChannel;

class UDPChannelListenerIOHandler final : public IOHandler
{
public:
    static std::shared_ptr<UDPChannelListenerIOHandler> Create(const Logger& logger,
                                                      ServerAcceptMode mode,
                                                      const std::shared_ptr<IChannelListener>& listener,
                                                      const std::shared_ptr<exe4cpp::StrandExecutor>& executor,
                                                      const IPEndpoint& localEndpoint,
                                                      std::shared_ptr<ISharedChannelData> sessionsManager)
    {
        return std::make_shared<UDPChannelListenerIOHandler>(logger, mode, listener, executor, localEndpoint, std::move(sessionsManager));
    }

    UDPChannelListenerIOHandler(const Logger& logger,
                       ServerAcceptMode mode,
                       const std::shared_ptr<IChannelListener>& listener,
                       std::shared_ptr<exe4cpp::StrandExecutor> executor,
                       IPEndpoint localEndpoint,
                       std::shared_ptr<ISharedChannelData> sessionsManager);

    void OnBeginRead(std::shared_ptr<UDPServerSocketChannel> remote, asio::ip::udp::endpoint channelEndpoint, ser4cpp::wseq_t dest);
    void OnBeginWrite(std::shared_ptr<UDPServerSocketChannel> remote, asio::ip::udp::endpoint channelEndpoint, const ser4cpp::rseq_t& buffer);

    bool AfterTransmit() override;

protected:
    void beginChannelAccept() override;
    void suspendChannelAccept() override;
    void shutdownImpl() override;
    void onChannelShutdown() override;

    bool tryOpen(const TimeDuration& delay) override;

private:
    void startServer();
    void stopServer();

    void readCallback(const std::error_code& ec, size_t num);

private:
    const IPEndpoint localEndpoint;
    asio::ip::udp::socket socket;
    asio::ip::udp::endpoint remote_endpoint;
    std::array<uint8_t, LPDU_MAX_FRAME_SIZE> buf;
};

} // namespace opendnp3
