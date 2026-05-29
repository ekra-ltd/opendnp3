#include "UDPServerSocketChannel.h"

namespace opendnp3
{

UDPServerSocketChannel::UDPServerSocketChannel(
    const std::shared_ptr<exe4cpp::StrandExecutor>& executor,
    const Logger& logger,
    UDPChannelListenerIOHandler* listener,
    asio::ip::udp::endpoint remote
)
    : IAsyncChannel(executor)
      , logger(logger)
      , listener(std::move(listener))
      , channelEndpoint(std::move(remote))
{
}

void UDPServerSocketChannel::OnRead(const std::error_code& ec, size_t num)
{
    if (this->isShutdown)
    {
        return;
    }
    this->OnReadCallback(ec, num);
}

void UDPServerSocketChannel::OnWrite(const std::error_code& ec, size_t num)
{
    if (this->isShutdown)
    {
        return;
    }
    this->OnWriteCallback(ec, num);
}

void UDPServerSocketChannel::BeginReadImpl(ser4cpp::wseq_t dest)
{
    if (this->isShutdown)
    {
        return;
    }
    this->listener->OnBeginRead(std::dynamic_pointer_cast<UDPServerSocketChannel>(shared_from_this()), this->channelEndpoint, dest);
}

void UDPServerSocketChannel::BeginWriteImpl(const ser4cpp::rseq_t& buffer)
{
    if (this->isShutdown)
    {
        return;
    }
    this->listener->OnBeginWrite(std::dynamic_pointer_cast<UDPServerSocketChannel>(shared_from_this()), this->channelEndpoint, buffer);
}

void UDPServerSocketChannel::ShutdownImpl()
{
    this->isShutdown = true;
}

} // namespace opendnp3
