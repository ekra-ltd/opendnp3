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

#include "channel/UDPSocketChannel.h"

#include "logging/LogMacros.h"

namespace opendnp3
{

UDPSocketChannel::UDPSocketChannel(const std::shared_ptr<exe4cpp::StrandExecutor>& executor,
                                   const Logger& logger, asio::ip::udp::socket socket)
    : IAsyncChannel(executor), logger(logger), socket(std::move(socket))
{
}

void UDPSocketChannel::BeginReadImpl(ser4cpp::wseq_t dest, const Addresses& addresses)
{
    auto callback = [this, addresses](const std::error_code& ec, size_t num) {
        this->OnReadCallback(ec, num, addresses);
    };

    socket.async_receive(asio::buffer(dest, dest.length()), this->executor->wrap(callback));
}

void UDPSocketChannel::BeginWriteImpl(const ser4cpp::rseq_t& buffer, const Addresses& addresses)
{
    auto callback = [this, addresses](const std::error_code& ec, size_t num) { this->OnWriteCallback(ec, num, addresses); };

    socket.async_send(asio::buffer(buffer, buffer.length()), this->executor->wrap(callback));
}

void UDPSocketChannel::ShutdownImpl()
{
    {
        std::error_code ec;
        socket.shutdown(asio::socket_base::shutdown_type::shutdown_both, ec);
        if (ec)
        {
            SIMPLE_LOG_BLOCK(logger, flags::ERR, ec.message().c_str());
        }
    }
    {
        std::error_code ec;
        socket.close(ec);
        if (ec)
        {
            SIMPLE_LOG_BLOCK(logger, flags::ERR, ec.message().c_str());
        }
    }
}

} // namespace opendnp3
