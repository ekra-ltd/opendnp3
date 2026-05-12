#include "IOHandlersManager.h"

#include "SharedChannelData.h"
#include "SerialIOHandler.h"
#include "TCPClientIOHandler.h"
#include "UDPClientIOHandler.h"
#include "logging/LogMacros.h"
#include <boost/thread/thread.hpp>

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
        const IOHandler::ConnectionFailureCallback_t callback = [this] {
            std::lock_guard<std::mutex> lock{ _mtx };
            prepareReconnect(false);
        };
        if (_primarySettings.UseTcp())
        {
            _primaryChannel = TCPClientIOHandler::Create(
                logger,
                listener,
                executor,
                retry,
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
                retry,
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
                retry,
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
                retry,
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
                retry,
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
                retry,
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
        if (result && _currentChannel)
        {
            _currentChannel->OnSessionRemoved();
        }
        
        return result;
    }

    bool IOHandlersManager::Enable(const std::shared_ptr<ILinkSession>& session) const
    {
        std::lock_guard<std::mutex> lock{ _mtx };
        const auto result = _sessionsManager->Enable(session);
        if (result && _currentChannel)
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
            if (_currentChannel)
            {
                _currentChannel->ConditionalClose();
            }
            if (_channelStateChanged)
            {
                _channelStateChanged(true);
            }
        }
        return result;
    }

    void IOHandlersManager::prepareReconnect(const bool onFail)
    {
        _succeededReadingCount = 0;
        (_backupChannelUsed ? _backupChannelState : _primaryChannelState) = Error;
        exe4cpp::duration_t duration{ 0 };
        FORMAT_LOG_BLOCK(
            _logger,
            flags::WARN,
            R"(%spreparing reconnect...)",
            onFail ? "connection error, " : ""
        )
        if (_primaryChannelState == Error && _backupChannelState == Error) {
            if (_channelStateChanged) {
                _channelStateChanged(true);
            }
            if (_backupSettings) {
                _backupChannelState = Undecided;
            }
            duration = _reconnectionDelay.value;
            FORMAT_LOG_BLOCK(
                _logger,
                flags::WARN,
                R"(retry scheduled in %lld seconds)",
                std::chrono::duration_cast<std::chrono::seconds>(duration).count()
            )
        }
        if (_currentChannel) {
            _currentChannel->Shutdown(false, true);
            _currentChannel.reset();
        }
        ChannelPaused(true);
        std::weak_ptr<IOHandlersManager> self = shared_from_this();
        _reconnectTimer = _executor->start(duration, [self] {
            if (const auto lockedSelf = self.lock()) {
                std::lock_guard<std::mutex> lock{ lockedSelf->_mtx };
                lockedSelf->tryReconnectChannel(true);
            }
        });
    }

    void IOHandlersManager::tryReconnectChannel(const bool withSwitch)
    {
        if (_currentChannel) {
            _currentChannel->Shutdown(false, true);
            _currentChannel.reset();
        }
        if (_backupSettings && withSwitch)
        {
            setIsBackupChannelUsed(!_backupChannelUsed);
        }
        FORMAT_LOG_BLOCK(
            _logger,
            flags::WARN,
            R"(trying to connect to %s channel...)",
            !_backupChannelUsed ? "primary" : "backup"
        )

        const auto settings = _backupChannelUsed ? *_backupSettings : _primarySettings;
        FORMAT_LOG_BLOCK(_logger, flags::DBG, R"(channel settings - %s)", settings.ToString().c_str())
        const auto newChannel = _backupChannelUsed ? _backupChannel : _primaryChannel;
        const auto handler = [newChannel, self = shared_from_this()] {
            std::lock_guard<std::mutex> lock{ self->_mtx };
            self->_succeededReadingCount = 0;
            self->_currentChannel = newChannel;
            self->ChannelPaused(false);
            self->ChannelReservationChanged(self->_backupChannelUsed);
        };
        if (newChannel->Prepare(handler))
        {
            handler();
        }
    }

    std::shared_ptr<IOHandler> IOHandlersManager::GetCurrent()
    {
        std::lock_guard<std::mutex> lock{ _mtx };
        return _currentChannel;
    }

    bool IOHandlersManager::PrepareChannel(bool canUseBackupChannel) const
    {
        std::lock_guard<std::mutex> lock{ _mtx };
        if (_backupChannelUsed && !canUseBackupChannel)
        {
            return false;
        }
        if (!_currentChannel)
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
                if (_backupChannelUsed) {
                    _primaryChannelState = Undecided;
                    _backupChannelState = Working;
                    tryReturnToPrimary();
                }
                else {
                    _primaryChannelState = Working;
                    if (_backupSettings) {
                        _backupChannelState = Undecided;
                    }
                }
                if (_channelStateChanged)
                {
                    _channelStateChanged(false);
                }
            }
        }
        else
        {
            prepareReconnect(true);
        }
    }

    LinkStatistics IOHandlersManager::Statistics() const
    {
        std::lock_guard<std::mutex> lock{ _mtx };
        if (!_currentChannel)
        {
            return {};
        }
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
        setIsBackupChannelUsed(false);
        _succeededReadingCount = 0;
        shutdown();
    }

    void IOHandlersManager::Shutdown()
    {
        std::lock_guard<std::mutex> lock{ _mtx };
        shutdown();
    }

    void IOHandlersManager::SetChannelStateChangedCallback(const Callback_t& afterCurrentChannelShutdown)
    {
        _channelStateChanged = afterCurrentChannelShutdown;
    }

    void IOHandlersManager::SetChannelRetryCount(const NumRetries& numRetries) const
    {
        _primaryChannel->SetChannelRetryCount(numRetries);
        if (_backupChannel) {
            _backupChannel->SetChannelRetryCount(numRetries);
        }
    }

    void IOHandlersManager::SetChannelReconnectionDelay(const TimeDuration& delay)
    {
        if (_reconnectionDelay.value > delay.value)
        {
            return;
        }
        _reconnectionDelay = delay;
    }

    bool IOHandlersManager::IsBackupChannelUsed() const
    {
        return _backupChannelUsed;
    }

    bool IOHandlersManager::CanSwitchChannel() const
    {
        return _backupChannelState != Error || _primaryChannelState != Error;
    }

    void IOHandlersManager::setIsBackupChannelUsed(bool value)
    {
        _backupChannelUsed = value;
        IsBackupChannelUsedChanged(_backupChannelUsed);
    }

    void IOHandlersManager::shutdown()
    {
        _reconnectTimer.cancel();
        ChannelReservationChanged.disconnect_all_slots();
        _primaryChannel->Shutdown(false);
        if (_backupChannel) {
            _backupChannel->Shutdown(false);
        }
    }

    void IOHandlersManager::tryReturnToPrimary()
    {
        // if we using backup channel - check the number of successful data reading (interrogations) to determine the necessity of switching to primary channel
        if (_succeededReadingCount >= _backupSettings->ReadingCountBeforeReturnToPrimary()) {
            FORMAT_LOG_BLOCK(
                _logger,
                flags::DBG,
                R"(Succeeded reading count %d >= %d)",
                _succeededReadingCount,
                _backupSettings->ReadingCountBeforeReturnToPrimary()
            )
            auto callback = [self = shared_from_this(), this] {
                std::lock_guard<std::mutex> lock{ _mtx };
                if (_backupChannelUsed)
                {
                    _succeededReadingCount = 0;
                    ChannelPaused(true);
                    tryReconnectChannel(true);
                }
            };
            _executor->post(_executor->wrap(callback));
        }
    }

} // namespace opendnp3
