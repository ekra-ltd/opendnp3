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
    {
        IOHandler::ConnectionFailureCallback_t callback = [this] {
            NotifyTaskResult(false, false);
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
                callback
            );
        }
        _currentChannel = _primaryChannel;

        if (!_backupSettings || !_backupSettings->IsBackupChannel())
        {
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

    std::shared_ptr<IOHandler> IOHandlersManager::GetCurrent()
    {
        std::lock_guard<std::mutex> lock{ _mtx };
        if (!_backupSettings || !_backupSettings->Enabled()) {
            return _primaryChannel;
        }

        // if we using backup channel - check the number of successful data reading (interrogations) to determine the necessity of switching to primary channel
        if (_backupChannelUsed) {
            if (_succeededReadingCount >= _backupSettings->ReadingCountBeforeReturnToPrimary()) {
                FORMAT_LOG_BLOCK(_logger, flags::DBG, R"(try switch to primary connection, {%s})", _primarySettings.ToString().c_str())
                _backupChannelUsed = false;
                _succeededReadingCount = 0;
            }
        }

        const auto newChannel = _backupChannelUsed ? _backupChannel : _primaryChannel;
        if (_currentChannel != newChannel)
        {
            _currentChannel->Shutdown();
            if (_afterCurrentChannelShutdown)
            {
                _afterCurrentChannelShutdown();
            }
            newChannel->Prepare();
            _currentChannel = newChannel;
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

            FORMAT_LOG_BLOCK(_logger, flags::DBG, R"(connection error, try switch to %s connection)", _backupChannelUsed ? "primary" : "backup")
            _backupChannelUsed = !_backupChannelUsed;
            const auto settings = _backupChannelUsed ? *_backupSettings : _primarySettings;
            FORMAT_LOG_BLOCK(_logger, flags::DBG, R"(channel settings - %s)", settings.ToString().c_str())
            const auto newChannel = _backupChannelUsed ? _backupChannel : _primaryChannel;
            if (_currentChannel != newChannel)
            {
                _currentChannel->Shutdown(true);
                if (_afterCurrentChannelShutdown)
                {
                    _afterCurrentChannelShutdown();
                }
                newChannel->Prepare();
                _currentChannel = newChannel;
            }
        }
    }

    LinkStatistics IOHandlersManager::Statistics() const
    {
        std::lock_guard<std::mutex> lock{ _mtx };
        return _currentChannel->Statistics();
    }

    void IOHandlersManager::AddStatisticsHandler(const StatisticsChangeHandler_t& statisticsChangeHandler) const
    {
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

    void IOHandlersManager::Shutdown() const
    {
        _primaryChannel->Shutdown(false);
        if (_backupChannel)
        {
            _backupChannel->Shutdown(false);
        }
    }

    void IOHandlersManager::SetShutdownCallback(const Callback_t& afterCurrentChannelShutdown)
    {
        _afterCurrentChannelShutdown = afterCurrentChannelShutdown;
    }

    bool IOHandlersManager::IsBackupChannelUsed() const
    {
        return _backupChannelUsed;
    }

} // namespace opendnp3
