#pragma once

#include "IOHandler.h"
#include "IoSessionDescriptor.h"
#include "opendnp3/channel/ChannelRetry.h"
#include "opendnp3/channel/ChannelConnectionOptions.h"
#include "opendnp3/link/LinkStatistics.h"
#include "opendnp3/logging/Logger.h"
#include <boost/optional/optional.hpp>
#include <boost/signals2/signal.hpp>

#include <mutex>

namespace opendnp3
{

    class IOHandlersManager final : public std::enable_shared_from_this<IOHandlersManager>
    {
        using Callback_t = std::function<void(bool)>;

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
        void Shutdown();

        void SetChannelStateChangedCallback(const Callback_t& afterCurrentChannelShutdown);

        bool IsBackupChannelUsed() const;

        using ChannelReservationChangedHandler_t = void(bool isBackup);
        using ChannelReservationChangedSignal_t = boost::signals2::signal<ChannelReservationChangedHandler_t>;
        ChannelReservationChangedSignal_t ChannelReservationChanged;

        using ChannelChangingHandler_t = void(bool);
        using ChannelChangingSignal_t = boost::signals2::signal<ChannelChangingHandler_t>;
        ChannelChangingSignal_t ChannelChanging;

    private:
        void trySwitchChannel(bool onFail);

    private:
        enum ChannelState
        {
            Working,
            Error,
            Undecided
        };

        mutable std::mutex _mtx;
        Logger _logger;
        ChannelConnectionOptions _primarySettings;
        boost::optional<ChannelConnectionOptions> _backupSettings;
        bool _backupChannelUsed{ false };
        unsigned _succeededReadingCount{ 0 };
        std::shared_ptr<IOHandler> _primaryChannel;
        std::shared_ptr<IOHandler> _backupChannel;
        std::shared_ptr<IOHandler> _oldChannel;
        std::shared_ptr<IOHandler> _currentChannel;
        std::shared_ptr<ISharedChannelData> _sessionsManager;
        Callback_t _channelStateChanged;
        std::shared_ptr<exe4cpp::StrandExecutor> _executor;
        ChannelState _primaryChannelState{ Error };
        ChannelState _backupChannelState{ Error };
    };

} // namespace opendnp3

