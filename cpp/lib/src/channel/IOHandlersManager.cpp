#include "IOHandlersManager.h"

#include "SharedChannelData.h"
#include "SerialIOHandler.h"
#include "TCPClientIOHandler.h"
#include "UDPClientIOHandler.h"
#include "logging/LogMacros.h"

#include <utility>

namespace opendnp3
{
    IOHandlersManager::IOHandlersManager(
        const Logger& logger,
        const std::shared_ptr<IChannelListener>& listener,
        const std::shared_ptr<exe4cpp::StrandExecutor>& executor,
        const ChannelRetry& retry,
        ChannelConnectionOptions primarySettings,
        boost::optional<ChannelConnectionOptions> backupSettings,
        const std::string& adapter
    )
        : _logger(logger)
        , _primarySettings(std::move(primarySettings))
        , _backupSettings(std::move(backupSettings))
        , _sessionsManager( std::make_shared<SharedChannelData>(_logger) )
        , _executor(executor)
    {
        IOHandler::ConnectionFailureCallback_t callback = [this] {
            std::lock_guard<std::mutex> lock{ _mtx };
            _succeededReadingCount = 0;
            (_backupChannelUsed ? _backupChannelState : _primaryChannelState) = Error;
            if (_primaryChannelState == Error && _backupChannelState == Error)
            {
                if (_channelStateChanged)
                {
                    _channelStateChanged(true);
                }
            }
            if (_backupSettings)
            {
                _backupChannelUsed = !_backupChannelUsed;
                if (_currentChannel)
                {
                    _currentChannel->Shutdown(false, true);
                    _currentChannel.reset();
                }
                trySwitchChannel(false);
            }
        };
        auto retrySetting = retry;
        retrySetting.InfiniteTries(!_backupSettings.has_value());
        if (_primarySettings.UseTcp())
        {
            _primaryChannel = TCPClientIOHandler::Create(
                logger,
                listener,
                executor,
                retrySetting,
                _primarySettings.TcpPortParameters(),
                adapter,
                _sessionsManager,
                true,
                callback
            );
        }
        else if (_primarySettings.UseSerial())
        {
            _primaryChannel = SerialIOHandler::Create(
                logger,
                listener,
                executor,
                retrySetting,
                _primarySettings.SerialPortParameters(),
                _sessionsManager,
                true,
                callback
            );
        }
        else
        {
            const auto udpSettings = _primarySettings.UDPParameters();
            _primaryChannel = UDPClientIOHandler::Create(
                logger,
                listener,
                executor,
                retrySetting,
                udpSettings.Local,
                udpSettings.Remote,
                _sessionsManager,
                true,
                callback
            );
        }
        _currentChannel = _primaryChannel;

        if (!_backupSettings || !_backupSettings->IsBackupChannel())
        {
            _backupChannelState = Error;
            _backupSettings = boost::none;
            return;
        }

        if (_backupSettings->UseTcp())
        {
            _backupChannel = TCPClientIOHandler::Create(
                logger,
                listener, 
                executor,
                retrySetting,
                _backupSettings->TcpPortParameters(),
                adapter,
                _sessionsManager,
                false,
                callback
            );
        }
        else if (_backupSettings->UseSerial())
        {
            _backupChannel = SerialIOHandler::Create(
                logger,
                listener,
                executor,
                retrySetting,
                _backupSettings->SerialPortParameters(),
                _sessionsManager,
                false,
                callback
            );
        }
        else
        {
            const auto udpSettings = _backupSettings->UDPParameters();
            _backupChannel = UDPClientIOHandler::Create(
                logger,
                listener,
                executor,
                retrySetting,
                udpSettings.Local,
                udpSettings.Remote,
                _sessionsManager,
                false,
                callback
            );
        }
    }

    IOHandlersManager::IOHandlersManager(
        const Logger& logger,
        const std::shared_ptr<IOHandler>& handler,
        std::shared_ptr<ISharedChannelData> sessionsManager
    )
        : _logger(logger)
        , _sessionsManager(std::move(sessionsManager))
    {
        _currentChannel = _primaryChannel = handler;
    }

    bool IOHandlersManager::AddContext(const std::shared_ptr<ILinkSession>& session, const Addresses& addresses) const
    {
        std::lock_guard<std::mutex> lock{ _mtx };
        return _sessionsManager->AddContext(session, addresses);
    }

    bool IOHandlersManager::Remove(const std::shared_ptr<ILinkSession>& session) const
    {
        std::lock_guard<std::mutex> lock{ _mtx };
        const bool result = _sessionsManager->Remove(session);
        if (result)
        {
            _currentChannel->OnSessionRemoved();
        }
        
        return result;
    }

    bool IOHandlersManager::Enable(const std::shared_ptr<ILinkSession>& session) const
    {
        std::lock_guard<std::mutex> lock{ _mtx };
        const auto result = _sessionsManager->Enable(session);
        if (result)
        {
            _currentChannel->Prepare();
        }
        return result;
    }

    bool IOHandlersManager::Disable(const std::shared_ptr<ILinkSession>& session) const
    {
        std::lock_guard<std::mutex> lock{ _mtx };
        const auto result = _sessionsManager->Disable(session);
        if (result)
        {
            _currentChannel->ConditionalClose();
        }
        return result;
    }

    void IOHandlersManager::trySwitchChannel(bool onFail)
    {
        if (!_backupSettings)
        {
            return;
        }

        FORMAT_LOG_BLOCK(
            _logger,
            flags::DBG,
            R"(%strying to switch to %s connection)",
            onFail ? "connection error, " : "",
            !_backupChannelUsed ? "primary" : "backup"
        )
        const auto settings = _backupChannelUsed ? *_backupSettings : _primarySettings;
        FORMAT_LOG_BLOCK(_logger, flags::DBG, R"(channel settings - %s)", settings.ToString().c_str())

        const auto newChannel = _backupChannelUsed ? _backupChannel : _primaryChannel;

        if (_currentChannel != newChannel)
        {
            ChannelChanging(true);
            _oldChannel = _currentChannel;
            const auto handler = [onFail, newChannel, self = shared_from_this()] {
                self->_succeededReadingCount = 0;
                if (self->_oldChannel)
                {
                    // shutdown without notifications
                    self->_oldChannel->Shutdown(onFail, true);
                    self->_oldChannel.reset();
                }
                self->_currentChannel = newChannel;
                self->ChannelChanging(false);
                if (self->_channelStateChanged)
                {
                    self->_channelStateChanged(false);
                }

                self->ChannelReservationChanged(self->_backupChannelUsed);
                (self->_backupChannelUsed ? self->_backupChannelState : self->_primaryChannelState) = Working;
                (!self->_backupChannelUsed ? self->_backupChannelState : self->_primaryChannelState) = Undecided;
            };
            if (newChannel->Prepare(handler))
            {
                handler();
            }
        }
    }

    std::shared_ptr<IOHandler> IOHandlersManager::GetCurrent()
    {
        std::lock_guard<std::mutex> lock{ _mtx };
        if (!_backupSettings || !_backupSettings->Enabled()) {
            return _primaryChannel;
        }

        // if we using backup channel - check the number of successful data reading (interrogations) to determine the necessity of switching to primary channel
        const auto oldBackupChannelUsed = _backupChannelUsed;
        if (_backupChannelUsed) {
            if (_succeededReadingCount >= _backupSettings->ReadingCountBeforeReturnToPrimary()) {
                FORMAT_LOG_BLOCK(
                    _logger,
                    flags::DBG,
                    R"(Succeeded reading count %d >= %d)",
                    _succeededReadingCount,
                    _backupSettings->ReadingCountBeforeReturnToPrimary()
                )
                _backupChannelUsed = false;
                _succeededReadingCount = 0;
            }
        }

        if (oldBackupChannelUsed != _backupChannelUsed) {
            trySwitchChannel(false);
        }
        return _currentChannel;
    }

    bool IOHandlersManager::PrepareChannel(bool canUseBackupChannel) const
    {
        std::lock_guard<std::mutex> lock{ _mtx };
        if (_backupChannelUsed && !canUseBackupChannel)
        {
            return false;
        }
        return _currentChannel->Prepare();
    }

    void IOHandlersManager::NotifyTaskResult(bool complete, bool isDataReading)
    {
        std::lock_guard<std::mutex> lock{ _mtx };
        if (complete)
        {
            if (isDataReading)
            {
                ++_succeededReadingCount;
            }
        }
        else
        {
            _succeededReadingCount = 0;
            if (!_backupSettings)
            {
                return;
            }

            (_backupChannelUsed ? _backupChannelState : _primaryChannelState) = Error;
            _backupChannelUsed = !_backupChannelUsed;
            trySwitchChannel(true);
        }
    }

    LinkStatistics IOHandlersManager::Statistics() const
    {
        std::lock_guard<std::mutex> lock{ _mtx };
        return _currentChannel->Statistics();
    }

    void IOHandlersManager::AddStatisticsHandler(const StatisticsChangeHandler_t& statisticsChangeHandler) const
    {
        std::lock_guard<std::mutex> lock{ _mtx };
        auto primaryHandler = [statisticsChangeHandler](const bool /*isBackupChannel*/, StatisticsValueType type, int64_t value) {
            if (statisticsChangeHandler) {
                statisticsChangeHandler(false, type, value);
            }
        };
        _primaryChannel->AddStatisticsHandler(primaryHandler);
        if (_backupChannel)
        {
            auto backupHandler = [statisticsChangeHandler](const bool /*isBackupChannel*/, StatisticsValueType type, int64_t value) {
                if (statisticsChangeHandler) {
                    statisticsChangeHandler(true, type, value);
                }
            };
            _backupChannel->AddStatisticsHandler(backupHandler);
        }
    }

    void IOHandlersManager::RemoveStatisticsHandler() const
    {
        std::lock_guard<std::mutex> lock{ _mtx };
        _primaryChannel->RemoveStatisticsHandler();
        if (_backupChannel)
        {
            _backupChannel->RemoveStatisticsHandler();
        }
    }

    void IOHandlersManager::Reset()
    {
        std::lock_guard<std::mutex> lock{ _mtx };
        _backupChannelUsed = false;
        _succeededReadingCount = 0;
        Shutdown();
    }

    void IOHandlersManager::Shutdown()
    {
        ChannelReservationChanged.disconnect_all_slots();
        _primaryChannel->Shutdown(false);
        if (_backupChannel)
        {
            _backupChannel->Shutdown(false);
        }
    }

    void IOHandlersManager::SetChannelStateChangedCallback(const Callback_t& afterCurrentChannelShutdown)
    {
        _channelStateChanged = afterCurrentChannelShutdown;
    }

    bool IOHandlersManager::IsBackupChannelUsed() const
    {
        return _backupChannelUsed;
    }

} // namespace opendnp3
