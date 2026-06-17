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

void UDPChannelListenerIOHandler::OnBeginRead(std::shared_ptr<UDPServerSocketChannel> remote, ser4cpp::wseq_t dest, const Addresses& addresses)
{
    auto cb = [=, self = shared_from_this()](const std::error_code& ec, size_t num)
    {
        if (ec && (ec.value() == asio::error::connection_refused || ec.value() == asio::error::connection_reset)) {
            // Ignore "connection_refused" error only for UDP.
            // Windows sends error 10061 if the remote endpoint is not bind on specified port.
            this->OnBeginRead(remote, dest, addresses);
            return;
        }
        remote->OnRead(ec, num, addresses);
    };
    socket.async_receive_from(asio::buffer(dest, dest.length()), remote_endpoint, this->executor->wrap(cb));
}

void UDPChannelListenerIOHandler::OnBeginWrite(std::shared_ptr<UDPServerSocketChannel> remote, const ser4cpp::rseq_t& buffer, const Addresses& addresses)
{
    auto cb = [remote, addresses](const std::error_code& ec, size_t num)
    {
        remote->OnWrite(ec, num, addresses);
    };
    socket.async_send_to(asio::buffer(buffer, buffer.length()), remote_endpoint, this->executor->wrap(cb));
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
    // do nothing
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
    if (ec)
    {
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
        return;
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

void UDPChannelListenerIOHandler::readCallback(const std::error_code& ec, size_t /*num*/)
{
    if (ec)
    {
        if (ec && (ec.value() == asio::error::connection_refused || ec.value() == asio::error::connection_reset))
        {
            socket.async_receive_from(asio::buffer(buf, buf.size()), remote_endpoint, asio::ip::udp::socket::message_peek, executor->wrap([self = shared_from_this(), this](const std::error_code& ec, size_t num) {
                readCallback(ec, num);
            }));
            return;
        }
        if (ec.value() != asio::error::operation_aborted)
        {
            SIMPLE_LOG_BLOCK(logger, flags::WARN, ec.message().c_str());
        }
    }
    else
    {
        onNewChannel(UDPServerSocketChannel::Create(this->executor, this->logger, std::dynamic_pointer_cast<UDPChannelListenerIOHandler>(shared_from_this())));
    }
}

} // namespace opendnp3
