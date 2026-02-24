#include "UDPChannelListener.h"
#include "channel/SocketHelpers.h"
#include "logging/LogMacros.h"

#include "opendnp3/ErrorCodes.h"

namespace opendnp3
{

UDPChannelListener::UDPChannelListener(
    Logger& logger,
    std::shared_ptr<exe4cpp::StrandExecutor> executor
)
    : logger(logger)
    , executor(executor)
    , socket(*executor->get_context())
    , finished(false)
    , isShutdown(false)
{
}

void UDPChannelListener::Bind(const IPEndpoint& localEndpoint, std::error_code& ec)
{
    SocketHelpers::BindToLocalAddress<asio::ip::udp>(localEndpoint.address, localEndpoint.port, socket, ec);
    if (ec)
    {
        FORMAT_LOG_BLOCK(logger, flags::WARN, "Failed to bind UDP socket to address=%s port=%u with error: %s",
                         localEndpoint.address.c_str(), localEndpoint.port, ec.message().c_str());
        throw DNP3Error(Error::UNABLE_TO_BIND_SERVER, ec);
    }
    StartReceive();
}

void UDPChannelListener::Shutdown()
{
    if (isShutdown)
    {
        return;
    }
    isShutdown = true;

    if (!finished)
    {
        {
            std::error_code ec;
            socket.shutdown(asio::socket_base::shutdown_both, ec);
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
}

void UDPChannelListener::StartReceive()
{
    auto callback = [self = shared_from_this()](const std::error_code& ec, std::size_t /*bytes_transferred*/)
    {
        if (self->isShutdown)
        {
            return;
        }
        if (!ec)
        {
            self->Finish();
        }
        else
        {
            SIMPLE_LOG_BLOCK(self->logger, flags::WARN, ec.message().c_str());
        }
    };
    socket.async_receive_from(asio::buffer(buf, buf.size()), remote_endpoint, asio::ip::udp::socket::message_peek, executor->wrap(callback));
}

void UDPChannelListener::Finish()
{
    std::error_code ec;
    // async_connect on udp socket also creates tcp connection to localhost ephemeral ports for some reason
    socket.connect(remote_endpoint, ec);
    if (!ec)
    {
        try
        {
            OnSocketReady(std::move(socket));
        }
        catch (const std::exception& e)
        {
            FORMAT_LOG_BLOCK(logger, flags::WARN, "Failed on creating UDP channel with error: %s", e.what());
        }
        finished = true;
    }
}

} // namespace opendnp3
