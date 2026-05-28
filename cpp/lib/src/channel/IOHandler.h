/*
 * Copyright 2013-2022 Step Function I/O, LLC
 *
 * Licensed to Green Energy Corp (www.greenenergycorp.com) and Step Function I/O
 * LLC (https://stepfunc.io) under one or more contributor license agreements.
 * See the NOTICE file distributed with this work for additional information
 * regarding copyright ownership. Green Energy Corp and Step Function I/O LLC license
 * this file to you under the Apache License, Version 2.0 (the "License"); you
 * may not use this file except in compliance with the License. You may obtain
 * a copy of the License at:
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#ifndef OPENDNP3_IOHANDLER_H
#define OPENDNP3_IOHANDLER_H

#include "ISharedChannelData.h"
#include "channel/IAsyncChannel.h"
#include "channel/IIOHandlerStatus.h"
#include "link/LinkContext.h"
#include "link/LinkLayerParser.h"

#include "opendnp3/channel/IChannelListener.h"
#include "opendnp3/logging/Logger.h"
#include "opendnp3/channel/ChannelRetry.h"

#include <boost/signals2/signal.hpp>

namespace opendnp3
{

/**

Manages I/O for a number of link contexts

*/
class IOHandler : private IFrameSink, public IChannelCallbacks, public IIOHandlerStatus, public std::enable_shared_from_this<IOHandler>
{
public:
    using ConnectionFailureCallback_t = std::function<void()>;
    using NewChannelOpenedCallback_t = std::function<void()>;

public:
    IOHandler(
        const Logger& logger,
        bool close_existing,
        std::shared_ptr<IChannelListener> listener,
        std::shared_ptr<ISharedChannelData> sessionsManager,
        bool isPrimary,
        std::shared_ptr<exe4cpp::StrandExecutor> executor,
        const ChannelRetry& channelRetry = ChannelRetry::Default(),
        ConnectionFailureCallback_t connectionFailureCallback = []{},
        TimeDuration holdChannelTimeout = {}
    );

    ~IOHandler() override = default;

    LinkStatistics Statistics() const;
    void ResetStatisticsCounters();

    void Shutdown(bool onFail = false, bool doNotNotify = false);
    bool IsShutdown() const override;

    /// --- implement ILinkTx ---

    bool BeginTransmit(const std::shared_ptr<ILinkSession>& session, const ser4cpp::rseq_t& data);

    // Begin sending messages to the context
    bool Prepare(const NewChannelOpenedCallback_t& channelOpenedCallback = nullptr);

    // Stop sending messages to this session
    void ConditionalClose();

    // Remove this session entirely
    bool OnSessionRemoved();

    void AddStatisticsHandler(const StatisticsChangeHandler_t& statisticsChangeHandler);
    void RemoveStatisticsHandler();

    void SetChannelRetryCount(const NumRetries& numRetries);

    void HoldChannel();

protected:
    // ------ Implement IChannelCallbacks -----

    void OnReadComplete(const std::error_code& ec, size_t num, const Addresses& addresses) final;

    void OnWriteComplete(const std::error_code& ec, size_t num, const Addresses& addresses) final;

    // ------ Super classes will implement these -----

    // start getting a new channel
    virtual void beginChannelAccept() = 0;

    // stop getting new channels
    virtual void suspendChannelAccept() = 0;

    // shutdown any additional state
    virtual void shutdownImpl() = 0;

    virtual bool checkOnShutdownInternal();

    // the current channel has closed, start getting a new one
    virtual void onChannelShutdown();

    // Called by the super class when a new channel is available
    void onNewChannel(const std::shared_ptr<IAsyncChannel>& newChannel);

    void resumeOnHoldChannel();

    virtual bool tryOpen(const TimeDuration& delay) = 0;

    virtual bool shouldRetry();
    virtual void performRetry(const std::shared_ptr<IOHandler>& self, const std::error_code& ec, const TimeDuration& delay);

    const bool close_existing;
    Logger logger;
    const std::shared_ptr<IChannelListener> listener;
    LinkStatistics::Channel statistics;
    ConnectionFailureCallback_t _connectionFailureCallback;
    std::atomic_bool _openingChannel{ false };
    NewChannelOpenedCallback_t _channelOpenedCallback;
    ChannelRetry retry;
    exe4cpp::Timer retryTimer; // connection retry timer
    const std::shared_ptr<exe4cpp::StrandExecutor> executor;

private:

    void UpdateListener(ChannelState state) const;

    // called by the parser when a complete frame is read
    bool OnFrame(const LinkHeaderFields& header, const ser4cpp::rseq_t& userdata) final;

    void Reset(bool onFail = true, bool doNotNotify = false);
    void BeginRead(const Addresses& addresses);
    bool CheckForSend(const Addresses& addresses);

    void logConnectionRetry();

    void notifyOpen(bool increment);
    void notifyClosed(bool increment);

    void startOnHoldChannelTimer();

private:
    bool isShutdown = false;

    LinkLayerParser parser;

    // current value of the channel, may be empty
    std::shared_ptr<IAsyncChannel> channel;

    std::shared_ptr<ISharedChannelData> _sessionsManager;

    mutable std::mutex _mtx;

    bool _isPrimary{ true };

    bool _isOpened{ false };

    exe4cpp::Timer _onHoldTimer;
    TimeDuration _holdChannelTimeout;
};

} // namespace opendnp3

#endif
