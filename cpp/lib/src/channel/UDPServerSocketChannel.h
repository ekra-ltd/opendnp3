#pragma once

#include "channel/IAsyncChannel.h"
#include "opendnp3/logging/Logger.h"

#include "channel/UDPChannelListenerIOHandler.h"

namespace opendnp3
{

class UDPServerSocketChannel final : public IAsyncChannel {
public:
    UDPServerSocketChannel(
        const std::shared_ptr<exe4cpp::StrandExecutor>& executor,
        const Logger& logger,
        UDPChannelListenerIOHandler* listener,
        asio::ip::udp::endpoint remote
    );

    static std::shared_ptr<IAsyncChannel> Create(
        const std::shared_ptr<exe4cpp::StrandExecutor>& executor,
        const Logger& logger,
        UDPChannelListenerIOHandler* listener,
        asio::ip::udp::endpoint remote
    )
    {
        return std::make_shared<UDPServerSocketChannel>(executor, logger, std::move(listener), std::move(remote));
    }

    ~UDPServerSocketChannel() override {
        this->logger.log(flags::WARN, __FILE__, "___ !!!!!!!!!!!!!");
    }

    void OnRead(const std::error_code& ec, size_t num);
    void OnWrite(const std::error_code& ec, size_t num);

protected:
    void BeginReadImpl(ser4cpp::wseq_t dest) override;
    void BeginWriteImpl(const ser4cpp::rseq_t& buffer) override;
    void ShutdownImpl() override;

private:
    Logger logger;
    UDPChannelListenerIOHandler* listener;
    asio::ip::udp::endpoint channelEndpoint;
    bool isShutdown = false;
};


} // namespace opendnp3
