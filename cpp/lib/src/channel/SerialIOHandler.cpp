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

#include "channel/SerialIOHandler.h"
#include <utility>

namespace opendnp3
{

SerialIOHandler::SerialIOHandler(
    const Logger& logger,
    const std::shared_ptr<IChannelListener>& listener,
    std::shared_ptr<exe4cpp::StrandExecutor> executor,
    const ChannelRetry& retry,
    SerialSettings settings,
    std::shared_ptr<ISharedChannelData> sessionsManager,
    bool isPrimary,
    ConnectionFailureCallback_t connectionFailureCallback,
    TimeDuration holdChannelTimeout
)
    : IOHandler(
        logger,
        false,
        listener,
        std::move(sessionsManager),
        isPrimary,
        std::move(executor),
        retry,
        std::move(connectionFailureCallback),
        holdChannelTimeout
    )
    , settings(std::move(settings))
{}

void SerialIOHandler::shutdownImpl()
{
    this->resetState();
}

void SerialIOHandler::beginChannelAccept()
{
    this->tryOpen(retry.minOpenRetry);
}

void SerialIOHandler::suspendChannelAccept()
{
    this->resetState();
}

bool SerialIOHandler::tryOpen(const TimeDuration& delay)
{
    auto callback = [self = shared_from_this(), delay, this] {
        std::error_code ec;
        const auto port = std::make_shared<SerialChannel>(executor);
        port->Open(settings, ec);
        if (ec)
        {
            performRetry(self, ec, delay);
        }
        else
        {
            this->onNewChannel(port);
        }
    };
    executor->post(callback);
    return true;
}

void SerialIOHandler::resetState()
{
    retryTimer.cancel();
}

} // namespace opendnp3
