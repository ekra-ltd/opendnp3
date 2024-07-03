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

#include "channel/IOHandler.h"

#include "logging/LogMacros.h"

#include "opendnp3/logging/LogLevels.h"

#include <utility>

namespace opendnp3
{

IOHandler::IOHandler(
    const Logger& logger,
    bool close_existing,
    std::shared_ptr<IChannelListener> listener,
    std::shared_ptr<ISharedChannelData> sessionsManager,
    ConnectionFailureCallback_t connectionFailureCallback
)
    : close_existing(close_existing)
    , logger(logger)
    , listener(std::move(listener))
    , _connectionFailureCallback(std::move(connectionFailureCallback))
    , parser(logger)
    , _sessionsManager(std::move(sessionsManager))
{
}

LinkStatistics IOHandler::Statistics() const
{
    std::lock_guard<std::mutex> lock{ _mtx };
    return { this->statistics, this->parser.Statistics() };
}

void IOHandler::Shutdown(bool onFail)
{
    std::lock_guard<std::mutex> lock{ _mtx };
    if (!isShutdown)
    {
        this->isShutdown = true;

        this->Reset(onFail);

        this->ShutdownImpl();

        this->UpdateListener(ChannelState::SHUTDOWN);
    }
}

void IOHandler::OnReadComplete(const std::error_code& ec, size_t num)
{
    if (ec)
    {
        FORMAT_LOG_BLOCK(this->logger, flags::WARN, "read error: %s", ec.message().c_str())

        this->Reset();

        this->UpdateListener(ChannelState::OPENING);
        this->OnChannelShutdown();
    }
    else
    {
        this->statistics.numBytesRx += num;

        this->parser.OnRead(num, *this);
        this->BeginRead();
    }
}

void IOHandler::OnWriteComplete(const std::error_code& ec, size_t num)
{
    if (ec)
    {
        FORMAT_LOG_BLOCK(this->logger, flags::WARN, "write error: %s", ec.message().c_str())
        this->Reset();

        this->UpdateListener(ChannelState::OPENING);
        this->OnChannelShutdown();
    }
    else
    {
        this->statistics.numBytesTx += num;

        if (!_sessionsManager->TxQueue().empty())
        {
            const auto session = _sessionsManager->TxQueue().front().Session;
            _sessionsManager->TxQueue().pop_front();
            session->OnTxReady();
        }

        this->CheckForSend();
    }
}

bool IOHandler::BeginTransmit(const std::shared_ptr<ILinkSession>& session, const ser4cpp::rseq_t& data)
{
    std::lock_guard<std::mutex> lock{ _mtx };
    if (this->channel)
    {
        _sessionsManager->TxQueue().emplace_back(data, session);
        return this->CheckForSend();
    }
    SIMPLE_LOG_BLOCK(logger, flags::ERR, "Router received transmit request while offline")
    return false;
}

bool IOHandler::Prepare()
{
    std::lock_guard<std::mutex> lock{ _mtx };
    if (_openingChannel)
    {
        return false;
    }
    this->isShutdown = false;
    if (!this->channel)
    {
        _openingChannel.exchange(true);
        this->UpdateListener(ChannelState::OPENING);

        this->BeginChannelAccept();
    }
    return true;
}

void IOHandler::ConditionalClose()
{
    std::lock_guard<std::mutex> lock{ _mtx };
    if (!_sessionsManager->IsAnySessionEnabled())
    {
        this->Reset(false);
        this->SuspendChannelAccept();
    }
}

bool IOHandler::OnSessionRemoved()
{
    std::lock_guard<std::mutex> lock{ _mtx };
    if (!_sessionsManager->IsAnySessionEnabled())
    {
        this->SuspendChannelAccept();
    }

    return true;
}

void IOHandler::OnNewChannel(const std::shared_ptr<IAsyncChannel>& newChannel)
{
    _openingChannel.exchange(false);
    // if we have an active channel, and we're configured to close new channels
    // close the new channel instead
    if (this->channel && !this->close_existing)
    {
        SIMPLE_LOG_BLOCK(this->logger, flags::WARN, "Existing channel, closing new connection")
        newChannel->Shutdown();
        return;
    }

    ++this->statistics.numOpen;

    this->Reset(false);

    this->channel = newChannel;

    this->channel->SetCallbacks(shared_from_this());

    this->UpdateListener(ChannelState::OPEN);

    this->BeginRead();

    _sessionsManager->LowerLayerUp();
    SIMPLE_LOG_BLOCK(logger, flags::DBG, "IOHandler, new channel opened")
}

void IOHandler::UpdateListener(ChannelState state) const
{
    if (listener)
    {
        listener->OnStateChange(state);
    }
}

bool IOHandler::OnFrame(const LinkHeaderFields& header, const ser4cpp::rseq_t& userdata)
{
    if (_sessionsManager->SendToSession(header.addresses, header, userdata))
    {
        return true;
    }

    FORMAT_LOG_BLOCK(this->logger, flags::WARN, "Frame w/ unknown route, source: %i, dest %i", header.addresses.source,
                     header.addresses.destination)
    return false;
}

void IOHandler::BeginRead()
{
    if (this->channel)
    {
        this->channel->BeginRead(this->parser.WriteBuff());
    }
}

bool IOHandler::CheckForSend()
{
    if (_sessionsManager->TxQueue().empty() || !this->channel || !this->channel->CanWrite())
    {
        return false;
    }

    ++this->statistics.numLinkFrameTx;
    return this->channel->BeginWrite(_sessionsManager->TxQueue().front().TxData);
}

void IOHandler::AddStatisticsHandler(const StatisticsChangeHandler_t& statisticsChangeHandler)
{
    std::lock_guard<std::mutex> lock{ _mtx };
    this->statistics.changeHandler = statisticsChangeHandler;
    this->parser.AddStatisticsHandler(statisticsChangeHandler);
}

void IOHandler::RemoveStatisticsHandler()
{
    std::lock_guard<std::mutex> lock{ _mtx };
    this->statistics.changeHandler = nullptr;
    this->parser.AddStatisticsHandler(nullptr);
}

void IOHandler::Reset(bool onFail)
{
    if (this->channel)
    {
        // shutdown the existing channel and drop the reference to it
        this->channel->Shutdown();
        this->channel.reset();

        if (onFail)
        {
            ++this->statistics.numClose;
        }

        this->UpdateListener(ChannelState::CLOSED);

        // notify any sessions that are online that this layer is offline
        _sessionsManager->LowerLayerDown();
    }

    // reset the state of the parser
    this->parser.Reset();

    // clear any pending tranmissions
    _sessionsManager->TxQueue().clear();
}

} // namespace opendnp3
