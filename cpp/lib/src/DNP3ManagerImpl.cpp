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

#include "DNP3ManagerImpl.h"

#include <utility>

#ifdef OPENDNP3_USE_TLS
#include "channel/tls/MasterTLSServer.h"
#include "channel/tls/TLSClientIOHandler.h"
#include "channel/tls/TLSServerIOHandler.h"
#endif

#include "channel/DNP3Channel.h"
#include "channel/IOHandlersManager.h"
#include "channel/SharedChannelData.h"
#include "channel/SerialIOHandler.h"
#include "channel/TCPClientIOHandler.h"
#include "channel/TCPServerIOHandler.h"
#include "channel/UDPClientIOHandler.h"
#include "master/MasterTCPServer.h"

#include "opendnp3/ErrorCodes.h"
#include "opendnp3/logging/LogLevels.h"

namespace opendnp3
{

DNP3ManagerImpl::DNP3ManagerImpl(uint32_t concurrencyHint,
                                 std::shared_ptr<ILogHandler> handler,
                                 std::function<void(uint32_t)> onThreadStart,
                                 std::function<void(uint32_t)> onThreadExit)
    : logger(std::move(handler), ModuleId(), "manager", levels::ALL),
      io(std::make_shared<asio::io_context>()),
      threadpool(io, concurrencyHint, std::move(onThreadStart), std::move(onThreadExit)),
      resources(ResourceManager::Create())
{
}

DNP3ManagerImpl::~DNP3ManagerImpl()
{
    this->Shutdown();
}

void DNP3ManagerImpl::Shutdown()
{
    if (resources)
    {
        resources->Shutdown();
        resources.reset();
    }
}

std::shared_ptr<IChannel> DNP3ManagerImpl::AddTCPClient(const std::string& id,
                                                        const LogLevels& levels,
                                                        const ChannelRetry& retry,
                                                        const ChannelConnectionOptions& primary,
                                                        const std::string& local,
                                                        std::shared_ptr<IChannelListener> listener) const
{
    auto create = [&]() -> std::shared_ptr<IChannel> {
        const auto clogger = this->logger.detach(id, levels);
        const auto executor = exe4cpp::StrandExecutor::create(this->io);
        const auto iohandlersManager = std::make_shared<IOHandlersManager>(clogger, listener, executor, retry, primary, boost::none, local);
        return DNP3Channel::Create(clogger, executor, iohandlersManager, this->resources);
    };

    auto channel = this->resources->Bind<IChannel>(create);

    if (!channel)
    {
        throw DNP3Error(Error::SHUTTING_DOWN);
    }

    return channel;
}

std::shared_ptr<IChannel> DNP3ManagerImpl::AddClientWithBackupChannel(
    const std::string& id,
    const opendnp3::LogLevels& levels,
    const ChannelRetry& retry,
    const ChannelConnectionOptions& primary,
    const boost::optional<ChannelConnectionOptions>& backup,
    const std::string& local,
    std::shared_ptr<IChannelListener> listener
) const
{
    auto create = [&]() -> std::shared_ptr<IChannel> {
        const auto clogger = this->logger.detach(id, levels);
        const auto executor = exe4cpp::StrandExecutor::create(this->io);
        const auto iohandlersManager = std::make_shared<IOHandlersManager>(clogger, listener, executor, retry, primary, backup, local);
        return DNP3Channel::Create(clogger, executor, iohandlersManager, this->resources);
    };

    auto channel = this->resources->Bind<IChannel>(create);

    if (!channel)
    {
        throw DNP3Error(Error::SHUTTING_DOWN);
    }

    return channel;
}

std::shared_ptr<IChannel> DNP3ManagerImpl::AddTCPServer(const std::string& id,
                                                        const LogLevels& levels,
                                                        ServerAcceptMode mode,
                                                        const TCPSettings& settings,
                                                        std::shared_ptr<IChannelListener> listener) const
{
    auto create = [&]() -> std::shared_ptr<IChannel> {
        std::error_code ec;
        auto clogger = this->logger.detach(id, levels);
        auto executor = exe4cpp::StrandExecutor::create(this->io);
        auto sessionManager = std::make_shared<SharedChannelData>(clogger);
        auto iohandler = TCPServerIOHandler::Create(clogger, mode, listener, executor, settings, ec, sessionManager);
        if (ec)
        {
            throw DNP3Error(Error::UNABLE_TO_BIND_SERVER, ec);
        }
        const auto iohandlersManager = std::make_shared<IOHandlersManager>(clogger, iohandler, sessionManager);
        return DNP3Channel::Create(clogger, executor, iohandlersManager, this->resources);
    };

    auto channel = this->resources->Bind<IChannel>(create);

    if (!channel)
    {
        throw DNP3Error(Error::SHUTTING_DOWN);
    }

    return channel;
}

std::shared_ptr<IChannel> DNP3ManagerImpl::AddUDPChannel(const std::string& id,
                                                         const LogLevels& levels,
                                                         const ChannelRetry& retry,
                                                         const IPEndpoint& localEndpoint,
                                                         const IPEndpoint& remoteEndpoint,
                                                         std::shared_ptr<IChannelListener> listener) const
{
    auto create = [&]() -> std::shared_ptr<IChannel> {
        auto clogger = this->logger.detach(id, levels);
        auto executor = exe4cpp::StrandExecutor::create(this->io);
        ChannelConnectionOptions primary{ UDPSettings(localEndpoint, remoteEndpoint) };
        const auto iohandlersManager = std::make_shared<IOHandlersManager>(clogger, listener, executor, retry, primary, boost::none);
        return DNP3Channel::Create(clogger, executor, iohandlersManager, this->resources);
    };

    auto channel = this->resources->Bind<IChannel>(create);

    if (!channel)
    {
        throw DNP3Error(Error::SHUTTING_DOWN);
    }

    return channel;
}

std::shared_ptr<IChannel> DNP3ManagerImpl::AddSerial(const std::string& id,
                                                     const LogLevels& levels,
                                                     const ChannelRetry& retry,
                                                     SerialSettings settings,
                                                     std::shared_ptr<IChannelListener> listener) const
{
    auto create = [&]() -> std::shared_ptr<IChannel> {
        auto clogger = this->logger.detach(id, levels);
        auto executor = exe4cpp::StrandExecutor::create(this->io);
        ChannelConnectionOptions primary(settings);
        const auto iohandlersManager = std::make_shared<IOHandlersManager>(clogger, listener, executor, retry, primary);
        return DNP3Channel::Create(clogger, executor, iohandlersManager, this->resources);
    };

    auto channel = this->resources->Bind<IChannel>(create);

    if (!channel)
    {
        throw DNP3Error(Error::SHUTTING_DOWN);
    }

    return channel;
}

std::shared_ptr<IChannel> DNP3ManagerImpl::AddTLSClient(const std::string& id,
                                                        const LogLevels& levels,
                                                        const ChannelRetry& retry,
                                                        const std::vector<IPEndpoint>& hosts,
                                                        const std::string& local,
                                                        const TLSConfig& config,
                                                        std::shared_ptr<IChannelListener> listener)
{

#ifdef OPENDNP3_USE_TLS
    auto create = [&]() -> std::shared_ptr<IChannel> {
        auto clogger = this->logger.detach(id, levels);
        auto executor = exe4cpp::StrandExecutor::create(this->io);
        auto sessionManager = std::make_shared<SharedChannelData>(clogger);
        auto iohandler = TLSClientIOHandler::Create(clogger, listener, executor, config, retry, IPEndpointsList{ hosts }, local, sessionManager);
        const auto iohandlersManager = std::make_shared<IOHandlersManager>(clogger, iohandler, sessionManager);
        return DNP3Channel::Create(clogger, executor, iohandlersManager, this->resources);
    };

    auto channel = this->resources->Bind<IChannel>(create);

    if (!channel)
    {
        throw DNP3Error(Error::SHUTTING_DOWN);
    }

    return channel;
#else
    throw DNP3Error(Error::NO_TLS_SUPPORT);
#endif
}

std::shared_ptr<IChannel> DNP3ManagerImpl::AddTLSServer(const std::string& id,
                                                        const LogLevels& levels,
                                                        ServerAcceptMode mode,
                                                        const IPEndpoint& endpoint,
                                                        const TLSConfig& config,
                                                        std::shared_ptr<IChannelListener> listener)
{

#ifdef OPENDNP3_USE_TLS
    auto create = [&]() -> std::shared_ptr<IChannel> {
        std::error_code ec;
        auto clogger = this->logger.detach(id, levels);
        auto executor = exe4cpp::StrandExecutor::create(this->io);
        auto sessionManager = std::make_shared<SharedChannelData>(clogger);
        auto iohandler = TLSServerIOHandler::Create(clogger, mode, listener, executor, endpoint, config, ec, sessionManager);
        if (ec)
        {
            throw DNP3Error(Error::UNABLE_TO_BIND_SERVER, ec);
        }
        const auto iohandlersManager = std::make_shared<IOHandlersManager>(clogger, iohandler, sessionManager);
        return DNP3Channel::Create(clogger, executor, iohandlersManager, this->resources);
    };

    auto channel = this->resources->Bind<IChannel>(create);

    if (!channel)
    {
        throw DNP3Error(Error::SHUTTING_DOWN);
    }

    return channel;

#else
    throw DNP3Error(Error::NO_TLS_SUPPORT);
#endif
}

std::shared_ptr<IListener> DNP3ManagerImpl::CreateListener(std::string loggerid,
                                                           const LogLevels& levels,
                                                           const IPEndpoint& endpoint,
                                                           const std::shared_ptr<IListenCallbacks>& callbacks) const
{
    auto create = [&]() -> std::shared_ptr<IListener> {
        std::error_code ec;
        auto server
            = MasterTCPServer::Create(this->logger.detach(loggerid, levels), exe4cpp::StrandExecutor::create(this->io),
                                      endpoint, callbacks, this->resources, ec);
        if (ec)
        {
            throw DNP3Error(Error::UNABLE_TO_BIND_SERVER, ec);
        }
        return server;
    };

    auto listener = this->resources->Bind<IListener>(create);

    if (!listener)
    {
        throw DNP3Error(Error::SHUTTING_DOWN);
    }

    return listener;
}

std::shared_ptr<IListener> DNP3ManagerImpl::CreateListener(std::string loggerid,
                                                           const LogLevels& levels,
                                                           const IPEndpoint& endpoint,
                                                           const TLSConfig& config,
                                                           const std::shared_ptr<IListenCallbacks>& callbacks)
{

#ifdef OPENDNP3_USE_TLS

    auto create = [&]() -> std::shared_ptr<IListener> {
        std::error_code ec;
        auto server
            = MasterTLSServer::Create(this->logger.detach(loggerid, levels), exe4cpp::StrandExecutor::create(this->io),
                                      endpoint, config, callbacks, this->resources, ec);
        if (ec)
        {
            throw DNP3Error(Error::UNABLE_TO_BIND_SERVER, ec);
        }
        return server;
    };

    auto listener = this->resources->Bind<IListener>(create);

    if (!listener)
    {
        throw DNP3Error(Error::SHUTTING_DOWN);
    }

    return listener;

#else
    throw DNP3Error(Error::NO_TLS_SUPPORT);
#endif
}

} // namespace opendnp3
