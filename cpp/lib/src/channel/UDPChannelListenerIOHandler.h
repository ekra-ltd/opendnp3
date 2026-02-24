#pragma once

#include "channel/IOHandler.h"
#include "channel/UDPChannelListener.h"

#include "opendnp3/channel/IPEndpointsList.h"
#include "opendnp3/gen/ServerAcceptMode.h"

#include <utility>

namespace opendnp3
{

class UDPChannelListenerIOHandler final : public IOHandler
{
    class Server final : public UDPChannelListener
    {
        using callback_t = std::function<void(asio::ip::udp::socket)>;

    public:
        Server(
            Logger& logger,
            std::shared_ptr<exe4cpp::StrandExecutor> executor,
            callback_t callback
        )
            : UDPChannelListener(logger, std::move(executor)),
              callback(std::move(callback))
        {
        }

        void Start(const IPEndpoint& localEndpoint, std::error_code& ec)
        {
            Bind(localEndpoint, ec);
        }

    protected:
        void OnSocketReady(asio::ip::udp::socket socket) override
        {
            callback(std::move(socket));
        }

    private:
        callback_t callback;
    };

public:
    static std::shared_ptr<UDPChannelListenerIOHandler> Create(const Logger& logger,
                                                      ServerAcceptMode mode,
                                                      const std::shared_ptr<IChannelListener>& listener,
                                                      const std::shared_ptr<exe4cpp::StrandExecutor>& executor,
                                                      const IPEndpoint& localEndpoint,
                                                      std::error_code& ec,
                                                      std::shared_ptr<ISharedChannelData> sessionsManager)
    {
        return std::make_shared<UDPChannelListenerIOHandler>(logger, mode, listener, executor, localEndpoint, ec, std::move(sessionsManager));
    }

    UDPChannelListenerIOHandler(const Logger& logger,
                       ServerAcceptMode mode,
                       const std::shared_ptr<IChannelListener>& listener,
                       const std::shared_ptr<exe4cpp::StrandExecutor>& executor,
                       const IPEndpoint& localEndpoint,
                       std::error_code& ec,
                       std::shared_ptr<ISharedChannelData> sessionsManager);

protected:
    void BeginChannelAccept() override;
    void SuspendChannelAccept() override;
    void ShutdownImpl() override;
    void OnChannelShutdown() override;

private:
    void startServer();
    void stopServer();
    void onNewChannel(asio::ip::udp::socket socket);

private:
    const std::shared_ptr<exe4cpp::StrandExecutor> executor;
    const IPEndpoint localEndpoint;
    std::shared_ptr<Server> server;
};

} // namespace opendnp3
