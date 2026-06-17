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
        std::shared_ptr<UDPChannelListenerIOHandler> listener
    );

    static std::shared_ptr<IAsyncChannel> Create(
        const std::shared_ptr<exe4cpp::StrandExecutor>& executor,
        const Logger& logger,
        std::shared_ptr<UDPChannelListenerIOHandler> listener
    )
    {
        return std::make_shared<UDPServerSocketChannel>(executor, logger, std::move(listener));
    }

    void OnRead(const std::error_code& ec, size_t num, const Addresses& addresses);
    void OnWrite(const std::error_code& ec, size_t num, const Addresses& addresses);

protected:
    void BeginReadImpl(ser4cpp::wseq_t dest, const Addresses& addresses) override;
    void BeginWriteImpl(const ser4cpp::rseq_t& buffer, const Addresses& addresses) override;
    void ShutdownImpl() override;

private:
    Logger logger;
    std::shared_ptr<UDPChannelListenerIOHandler> listener;
    bool isShutdown = false;
};


} // namespace opendnp3
