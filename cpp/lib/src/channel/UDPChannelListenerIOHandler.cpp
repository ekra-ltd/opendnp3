#include "UDPChannelListenerIOHandler.h"
#include "channel/SocketHelpers.h"
#include "channel/UDPServerSocketChannel.h"
#include "logging/LogMacros.h"

#include "opendnp3/ErrorCodes.h"

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
    : IOHandler(logger, mode == ServerAcceptMode::CloseExisting, listener, std::move(sessionsManager), true, executor)
    , localEndpoint(std::move(localEndpoint))
    , socket(*executor->get_context())
{}

void UDPChannelListenerIOHandler::OnBeginRead(std::shared_ptr<UDPServerSocketChannel> remote, asio::ip::udp::endpoint channelEndpoint, ser4cpp::wseq_t dest)
{
    auto cb = [=, self = shared_from_this()](const std::error_code& ec, size_t num) {
        if (remote_endpoint == channelEndpoint) {
            remote->OnRead(ec, num);
        }
        else {
            readCallback(ec, num);
        }
    };
    socket.async_receive_from(asio::buffer(dest, dest.length()), remote_endpoint, this->executor->wrap(cb));
}

void UDPChannelListenerIOHandler::OnBeginWrite(std::shared_ptr<UDPServerSocketChannel> remote, asio::ip::udp::endpoint channelEndpoint, const ser4cpp::rseq_t& buffer)
{
    auto cb = [remote](const std::error_code& ec, size_t num) {
        remote->OnWrite(ec, num);
    };
    socket.async_send_to(asio::buffer(buffer, buffer.length()), channelEndpoint, this->executor->wrap(cb));
}

bool UDPChannelListenerIOHandler::AfterTransmit()
{
    std::lock_guard<std::mutex> lock{ _mtx };
    // force close UDP channel after response, so we can take request from any UDP port.
    // it is what it is...
    restartChannel();
    return false;
}

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
    std::error_code ec;
    SocketHelpers::BindToLocalAddress<asio::ip::udp>(localEndpoint.address, localEndpoint.port, socket, ec);
    if (ec)
    {
        FORMAT_LOG_BLOCK(logger, flags::WARN, "Failed to bind UDP socket to address=%s port=%u with error: %s",
                         localEndpoint.address.c_str(), localEndpoint.port, ec.message().c_str());
        throw DNP3Error(Error::UNABLE_TO_BIND_SERVER, ec);
    }
    if (ec) {
        SIMPLE_LOG_BLOCK(logger, flags::WARN, ec.message().c_str())
        return false;
    }

    socket.async_receive_from(asio::buffer(buf, buf.size()), remote_endpoint, asio::ip::udp::socket::message_peek, executor->wrap([self = shared_from_this(), this](const std::error_code& ec, size_t num) {
        readCallback(ec, num);
    }));

    return true;
}

void UDPChannelListenerIOHandler::startServer()
{
    if (socket.is_open())
    {
        stopServer();
    }
    tryOpen(TimeDuration::Max());
}

void UDPChannelListenerIOHandler::stopServer()
{
    if (!socket.is_open())
    {
        return;
    }
    {
        std::error_code ec;
        socket.shutdown(asio::socket_base::shutdown_type::shutdown_both, ec);
        if (ec)
        {
            SIMPLE_LOG_BLOCK(logger, flags::ERR, ec.message().c_str());
        }
    }
    {
        std::error_code ec;
        socket.close(ec);
        if (ec)
        {
            SIMPLE_LOG_BLOCK(logger, flags::ERR, ec.message().c_str());
        }
    }
}

void UDPChannelListenerIOHandler::readCallback(const std::error_code& ec, size_t num)
{
    FORMAT_LOG_BLOCK(this->logger, flags::INFO, "UDP server socket new channel: %s, port %u, sending to %s, port %u",
                     socket.local_endpoint().address().to_string().c_str(), socket.local_endpoint().port(),
                     remote_endpoint.address().to_string().c_str(), remote_endpoint.port());
    onNewChannel(UDPServerSocketChannel::Create(this->executor, this->logger, this, remote_endpoint));
}

} // namespace opendnp3
