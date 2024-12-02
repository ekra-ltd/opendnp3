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
#ifndef OPENDNP3_TCPCLIENTIOHANDLER_H
#define OPENDNP3_TCPCLIENTIOHANDLER_H

#include "channel/IOHandler.h"
#include "channel/TCPClient.h"

#include "opendnp3/channel/ChannelRetry.h"
#include "opendnp3/channel/TCPSettings.h"

#include <exe4cpp/Timer.h>

namespace opendnp3
{

class TCPClientIOHandler final : public IOHandler
{

public:
    static std::shared_ptr<TCPClientIOHandler> Create(const Logger& logger,
                                                      const std::shared_ptr<IChannelListener>& listener,
                                                      const std::shared_ptr<exe4cpp::StrandExecutor>& executor,
                                                      const ChannelRetry& retry,
                                                      const TCPSettings& tcpSettings,
                                                      const std::string& adapter,
                                                      std::shared_ptr<ISharedChannelData> sessionsManager,
                                                      bool isPrimary,
                                                      ConnectionFailureCallback_t connectionFailureCallback = []{})
    {
        return std::make_shared<TCPClientIOHandler>(
            logger,
            listener,
            executor,
            retry,
            tcpSettings,
            adapter,
            std::move(sessionsManager),
            isPrimary,
            std::move(connectionFailureCallback)
        );
    }

    TCPClientIOHandler(const Logger& logger,
                       const std::shared_ptr<IChannelListener>& listener,
                       std::shared_ptr<exe4cpp::StrandExecutor> executor,
                       const ChannelRetry& retry,
                       const TCPSettings& tcpSettings,
                       std::string adapter,
                       std::shared_ptr<ISharedChannelData> sessionsManager,
                       bool isPrimary,
                       ConnectionFailureCallback_t connectionFailureCallback);

protected:
    void ShutdownImpl() override;
    void BeginChannelAccept() override;
    void SuspendChannelAccept() override;
    void OnChannelShutdown() override;

private:
    bool StartConnect(const TimeDuration& delay);

    void ResetState();

    const std::shared_ptr<exe4cpp::StrandExecutor> executor;
    const ChannelRetry retry;
    TCPSettings settings;
    const std::string adapter;

    // current value of the client
    std::shared_ptr<TCPClient> client;

    // connection retry timer
    exe4cpp::Timer retrytimer;
};

} // namespace opendnp3

#endif
