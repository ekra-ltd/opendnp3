#include "UDPChannelListenerIOHandler.h"
#include "channel/UDPSocketChannel.h"
#include "logging/LogMacros.h"

namespace opendnp3
{

UDPChannelListenerIOHandler::UDPChannelListenerIOHandler(
    const Logger& logger,
    ServerAcceptMode mode,
    const std::shared_ptr<IChannelListener>& listener,
    std::shared_ptr<exe4cpp::StrandExecutor> executor,
    IPEndpoint localEndpoint,
    std::shared_ptr<ISharedChannelData> sessionsManager
)
    : IOHandler(logger, mode == ServerAcceptMode::CloseExisting, listener, std::move(sessionsManager), true, std::move(executor))
    , localEndpoint(std::move(localEndpoint))
{}

void UDPChannelListenerIOHandler::beginChannelAccept()
{
    startServer();
}

void UDPChannelListenerIOHandler::suspendChannelAccept()
{
    stopServer();
}

void UDPChannelListenerIOHandler::shutdownImpl()
{
    stopServer();
}

void UDPChannelListenerIOHandler::onChannelShutdown()
{
    stopServer();
    startServer();
}

bool UDPChannelListenerIOHandler::tryOpen(const TimeDuration& /*delay*/)
{
    server = std::make_shared<Server>(logger, executor, [self = shared_from_this(), this](asio::ip::udp::socket socket) {
        onNewChannelInternal(std::move(socket));
    });
    std::error_code ec;
    server->Start(localEndpoint, ec);
    if (ec) {
        SIMPLE_LOG_BLOCK(logger, flags::WARN, ec.message().c_str())
        return false;
    }

    return true;
}

void UDPChannelListenerIOHandler::startServer()
{
    tryOpen(TimeDuration::Max());
}

void UDPChannelListenerIOHandler::stopServer()
{
    if (server) {
        server->Shutdown();
        server.reset();
    }
}

void UDPChannelListenerIOHandler::onNewChannelInternal(asio::ip::udp::socket socket)
{
    FORMAT_LOG_BLOCK(this->logger, flags::INFO, "UDP socket binded to: %s, port %u, sending to %s, port %u",
                     socket.local_endpoint().address().to_string().c_str(), socket.local_endpoint().port(),
                     socket.remote_endpoint().address().to_string().c_str(), socket.remote_endpoint().port())
    onNewChannel(UDPSocketChannel::Create(executor, logger, std::move(socket)));
}

} // namespace opendnp3
