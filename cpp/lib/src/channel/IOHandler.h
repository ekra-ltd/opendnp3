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
#include "link/ILinkTx.h"
#include "link/LinkLayerParser.h"
#include "master/IMasterTask.h"

#include "opendnp3/channel/IChannelListener.h"
#include "opendnp3/link/Addresses.h"
#include "opendnp3/logging/Logger.h"

#include <deque>
#include <vector>

namespace opendnp3
{

/**

Manages I/O for a number of link contexts

*/
class IOHandler : private IFrameSink, public IChannelCallbacks, public std::enable_shared_from_this<IOHandler>
{
public:
    using ConnectionFailureCallback_t = std::function<void()>;

public:
    IOHandler(
        const Logger& logger,
        bool close_existing,
        std::shared_ptr<IChannelListener> listener,
        std::shared_ptr<ISharedChannelData> sessionsManager,
        ConnectionFailureCallback_t connectionFailureCallback = []{}
    );

    ~IOHandler() override = default;

    LinkStatistics Statistics() const;

    void Shutdown(bool onFail = false);

    /// --- implement ILinkTx ---

    bool BeginTransmit(const std::shared_ptr<ILinkSession>& session, const ser4cpp::rseq_t& data);

    // Begin sending messages to the context
    bool Prepare();

    // Stop sending messages to this session
    void ConditionalClose();

    // Remove this session entirely
    bool OnSessionRemoved();

    void AddStatisticsHandler(const StatisticsChangeHandler_t& statisticsChangeHandler);
    void RemoveStatisticsHandler();

protected:
    // ------ Implement IChannelCallbacks -----

    void OnReadComplete(const std::error_code& ec, size_t num) final;

    void OnWriteComplete(const std::error_code& ec, size_t num) final;

    // ------ Super classes will implement these -----

    // start getting a new channel
    virtual void BeginChannelAccept() = 0;

    // stop getting new channels
    virtual void SuspendChannelAccept() = 0;

    // shutdown any additional state
    virtual void ShutdownImpl() = 0;

    // the current channel has closed, start getting a new one
    virtual void OnChannelShutdown() = 0;

    // Called by the super class when a new channel is available
    void OnNewChannel(const std::shared_ptr<IAsyncChannel>& newChannel);

    const bool close_existing;
    Logger logger;
    const std::shared_ptr<IChannelListener> listener;
    LinkStatistics::Channel statistics;
    ConnectionFailureCallback_t _connectionFailureCallback;
    std::atomic_bool _openingChannel{ false };

private:
    bool isShutdown = false;

    void UpdateListener(ChannelState state) const;

    // called by the parser when a complete frame is read
    bool OnFrame(const LinkHeaderFields& header, const ser4cpp::rseq_t& userdata) final;

    void Reset(bool onFail = true);
    void BeginRead();
    bool CheckForSend();

    LinkLayerParser parser;

    // current value of the channel, may be empty
    std::shared_ptr<IAsyncChannel> channel;

    std::shared_ptr<ISharedChannelData> _sessionsManager;

    mutable std::mutex _mtx;
};

} // namespace opendnp3

#endif
