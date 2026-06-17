#include "UDPServerSocketChannel.h"

namespace opendnp3
{

UDPServerSocketChannel::UDPServerSocketChannel(
    const std::shared_ptr<exe4cpp::StrandExecutor>& executor,
    const Logger& logger,
    std::shared_ptr<UDPChannelListenerIOHandler> listener
)
    : IAsyncChannel(executor)
      , logger(logger)
      , listener(std::move(listener))
{
}

void UDPServerSocketChannel::OnRead(const std::error_code& ec, size_t num, const Addresses& addresses)
{
    this->OnReadCallback(ec, num, addresses);
}

void UDPServerSocketChannel::OnWrite(const std::error_code& ec, size_t num, const Addresses& addresses)
{
    this->OnWriteCallback(ec, num, addresses);
}

void UDPServerSocketChannel::BeginReadImpl(ser4cpp::wseq_t dest, const Addresses& addresses)
{
    if (this->isShutdown)
    {
        return;
    }
    this->listener->OnBeginRead(std::dynamic_pointer_cast<UDPServerSocketChannel>(shared_from_this()), dest, addresses);
}

void UDPServerSocketChannel::BeginWriteImpl(const ser4cpp::rseq_t& buffer, const Addresses& addresses)
{
    if (this->isShutdown)
    {
        return;
    }
    this->listener->OnBeginWrite(std::dynamic_pointer_cast<UDPServerSocketChannel>(shared_from_this()), buffer, addresses);
}

void UDPServerSocketChannel::ShutdownImpl()
{
    this->isShutdown = true;
}

} // namespace opendnp3
