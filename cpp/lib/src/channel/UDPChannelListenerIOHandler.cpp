#include "UDPChannelListenerIOHandler.h"
#include "channel/UDPSocketChannel.h"
#include "logging/LogMacros.h"

namespace opendnp3
{

UDPChannelListenerIOHandler::UDPChannelListenerIOHandler(
    const Logger& logger,
    ServerAcceptMode mode,
    const std::shared_ptr<IChannelListener>& listener,
    const std::shared_ptr<exe4cpp::StrandExecutor>& executor,
    const IPEndpoint& localEndpoint,
    std::error_code& ec,
    std::shared_ptr<ISharedChannelData> sessionsManager
)
    : IOHandler(logger, mode == ServerAcceptMode::CloseExisting, listener, std::move(sessionsManager), true)
    , executor(executor)
    , localEndpoint(localEndpoint)
{
}

void UDPChannelListenerIOHandler::BeginChannelAccept()
{
    startServer();
}

void UDPChannelListenerIOHandler::SuspendChannelAccept()
{
    stopServer();
}

void UDPChannelListenerIOHandler::ShutdownImpl()
{
    stopServer();
}

void UDPChannelListenerIOHandler::OnChannelShutdown()
{
    stopServer();
    startServer();
}

void UDPChannelListenerIOHandler::startServer()
{
    std::error_code ec;
    server = std::make_shared<Server>(logger, executor, [self = shared_from_this(), this](asio::ip::udp::socket socket) {
        onNewChannel(std::move(socket));
    });
    server->Start(localEndpoint, ec);
    if (ec)
    {
        SIMPLE_LOG_BLOCK(logger, flags::WARN, ec.message().c_str());
    }
}

void UDPChannelListenerIOHandler::stopServer()
{
    if (server) {
        server->Shutdown();
        server.reset();
    }
}

void UDPChannelListenerIOHandler::onNewChannel(asio::ip::udp::socket socket)
{
    FORMAT_LOG_BLOCK(this->logger, flags::INFO, "UDP socket binded to: %s, port %u, sending to %s, port %u",
                     socket.local_endpoint().address().to_string().c_str(), socket.local_endpoint().port(),
                     socket.remote_endpoint().address().to_string().c_str(), socket.remote_endpoint().port());
    OnNewChannel(UDPSocketChannel::Create(executor, logger, std::move(socket)));
}

} // namespace opendnp3
