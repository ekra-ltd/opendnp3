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

#include "channel/ASIOTCPSocketHelpers.h"
#include "channel/TCPClientIOHandler.h"
#include "channel/TCPSocketChannel.h"

#include <utility>

namespace opendnp3
{

TCPClientIOHandler::TCPClientIOHandler(const Logger& logger,
                                       const std::shared_ptr<IChannelListener>& listener,
                                       std::shared_ptr<exe4cpp::StrandExecutor> executor,
                                       const ChannelRetry& retry,
                                       const TCPSettings& settings,
                                       std::string adapter,
                                       std::shared_ptr<ISharedChannelData> sessionsManager,
                                       bool isPrimary,
                                       ConnectionFailureCallback_t connectionFailureCallback)
    : IOHandler(logger, false, listener, std::move(sessionsManager), isPrimary, std::move(connectionFailureCallback)),
      executor(std::move(executor)),
      retry(retry),
      settings(settings),
      adapter(std::move(adapter))
{
}

void TCPClientIOHandler::ShutdownImpl()
{
    this->ResetState();
}

void TCPClientIOHandler::BeginChannelAccept()
{
    if (this->client)
    {
        this->client->Cancel();
    }
    this->client = TCPClient::Create(logger, executor, adapter);
    this->StartConnect(this->retry.minOpenRetry);
}

void TCPClientIOHandler::SuspendChannelAccept()
{
    this->ResetState();
}

void TCPClientIOHandler::OnChannelShutdown()
{
    if (this->retry.InfiniteTries())
    {
        this->retrytimer = this->executor->start(this->retry.reconnectDelay.value, [this, self = shared_from_this()]() {
            if (!client)
            {
                return;
            }
            this->BeginChannelAccept();
        });
    }
    else if (_connectionFailureCallback)
    {
        _openingChannel.exchange(false);
        _connectionFailureCallback();
    }
}

bool TCPClientIOHandler::StartConnect(const TimeDuration& delay)
{
    if (!client)
    {
        return false;
    }

    auto cb = [=, self = shared_from_this()](const std::shared_ptr<exe4cpp::StrandExecutor>& executor,
                                             asio::ip::tcp::socket socket, const std::error_code& ec) -> void {
        if (ec)
        {
            FORMAT_LOG_BLOCK(this->logger, flags::WARN, "Error Connecting: %s", ec.message().c_str())

            ++this->statistics.numOpenFail;

            const auto newDelay = this->retry.NextDelay(delay);

            if (client)
            {
                auto retry_cb = [self, newDelay, this]() {
                    if (this->retry.InfiniteTries() || this->settings.Endpoints.Next())
                    {
                        this->StartConnect(newDelay);
                    }
                    else if (_connectionFailureCallback)
                    {
                        _openingChannel.exchange(false);
                        _connectionFailureCallback();
                    }
                };

                this->retrytimer = this->executor->start(delay.value, retry_cb);
            }
        }
        else
        {
            FORMAT_LOG_BLOCK(this->logger, flags::INFO, "Connected to: %s, port %u",
                             this->settings.Endpoints.GetCurrentEndpoint().address.c_str(),
                             this->settings.Endpoints.GetCurrentEndpoint().port);

            if (client)
            {
                std::error_code keepAliveOptionsEc;
                ConfigureTCPKeepAliveOptions(settings, socket, keepAliveOptionsEc);
                if (keepAliveOptionsEc)
                {
                    FORMAT_LOG_BLOCK(this->logger, flags::WARN, "Error Configuring Keep-alive Options: %s",
                                     keepAliveOptionsEc.message().c_str())
                }
                this->OnNewChannel(TCPSocketChannel::Create(executor, std::move(socket)));
            }
        }
    };

    FORMAT_LOG_BLOCK(this->logger, flags::INFO, "Connecting to: %s, port %u",
                     this->settings.Endpoints.GetCurrentEndpoint().address.c_str(),
                     this->settings.Endpoints.GetCurrentEndpoint().port);

    this->client->BeginConnect(this->settings.Endpoints.GetCurrentEndpoint(), cb);

    return true;
}

void TCPClientIOHandler::ResetState()
{
    if (this->client)
    {
        this->client->Cancel();
        this->client.reset();
    }

    this->settings.Endpoints.Reset();

    retrytimer.cancel();
}

} // namespace opendnp3
