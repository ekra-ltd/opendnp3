#pragma once

#include "IOHandler.h"
#include "IoSessionDescriptor.h"
#include "opendnp3/channel/ChannelRetry.h"
#include "opendnp3/channel/ChannelConnectionOptions.h"
#include "opendnp3/link/LinkStatistics.h"
#include "opendnp3/logging/Logger.h"
#include <boost/optional/optional.hpp>

#include <mutex>

namespace opendnp3
{

    class IOHandlersManager final : public std::enable_shared_from_this<IOHandlersManager>
    {
        using Callback_t = std::function<void()>;

    public:
        IOHandlersManager(
            const Logger& logger,
            const std::shared_ptr<IChannelListener>& listener,
            const std::shared_ptr<exe4cpp::StrandExecutor>& executor,
            const ChannelRetry& retry,
            ChannelConnectionOptions primarySettings,
            boost::optional<ChannelConnectionOptions> backupSettings = {},
            const std::string& adapter = {}
        );

        IOHandlersManager(
            const Logger& logger,
            const std::shared_ptr<IOHandler>& handler,
            std::shared_ptr<ISharedChannelData> sessionsManager
        );

        bool AddContext(const std::shared_ptr<ILinkSession>& session, const Addresses& addresses) const;

        bool Enable(const std::shared_ptr<ILinkSession>& session) const;

        bool Disable(const std::shared_ptr<ILinkSession>& session) const;

        bool Remove(const std::shared_ptr<ILinkSession>& session) const;

        std::shared_ptr<IOHandler> GetCurrent();

        bool PrepareChannel(bool canUseBackupChannel) const;
        void NotifyTaskResult(bool complete, bool isDataReading);

        LinkStatistics Statistics() const;

        void AddStatisticsHandler(const StatisticsChangeHandler_t& statisticsChangeHandler) const;
        void RemoveStatisticsHandler() const;

        void Reset();
        void Shutdown() const;

        void SetShutdownCallback(const Callback_t& afterCurrentChannelShutdown);

        bool IsBackupChannelUsed() const;

    private:
        mutable std::mutex _mtx;
        Logger _logger;
        ChannelConnectionOptions _primarySettings;
        boost::optional<ChannelConnectionOptions> _backupSettings;
        bool _backupChannelUsed{ false };
        unsigned _succeededReadingCount{ 0 };
        std::shared_ptr<IOHandler> _primaryChannel;
        std::shared_ptr<IOHandler> _backupChannel;
        std::shared_ptr<IOHandler> _currentChannel;
        std::shared_ptr<ISharedChannelData> _sessionsManager;
        Callback_t _afterCurrentChannelShutdown;
    };

} // namespace opendnp3

