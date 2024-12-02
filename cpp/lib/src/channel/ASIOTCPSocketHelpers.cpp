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

#ifdef _WIN32
#include <WinSock2.h>
#else
#include <sys/types.h>
#include <sys/socket.h>
#endif
#include <cstdint>

namespace opendnp3
{

namespace
{

    template<
        typename Socket,
        typename Level,
        typename OptName,
        typename OptVal,
        typename OptLen
    >
    void SetSockOpt(
        Socket socket,
        Level level,
        OptName optName,
        OptVal &optVal,
        OptLen optLen,
        std::error_code& ec
    ) {
#ifdef _WIN32
        const auto res = setsockopt(
            static_cast<SOCKET>(socket), level, optName, reinterpret_cast<const char*>(&optVal), static_cast<int>(optLen)
        );
        if (res != 0) {
            ec = std::error_code{ WSAGetLastError(), std::system_category() };
        }
#else
        const auto res = setsockopt(
            static_cast<int>(socket), level, optName, &optVal, optLen
        );
        if (res != 0) {
            ec = std::error_code{ errno, std::system_category() };
        }
#endif
    }

} // namespace

bool ConfigureTCPKeepAliveOptions(
    const TCPSettings& settings,
    asio::ip::tcp::socket& socket,
    std::error_code& ec
) {
    ec = std::error_code{};
    if (settings.ForceKeepAlive) {
        std::error_code keepAliveEc;
        socket.set_option(asio::ip::tcp::socket::keep_alive{ true }, keepAliveEc);
        if (!ec) {
            ec = keepAliveEc;
        }
#ifdef TCP_KEEPIDLE
        const auto keepIdle = static_cast<uint32_t>(settings.KeepIdle.count());
        std::error_code keepIdleEc;
        SetSockOpt(
            socket.native_handle(), IPPROTO_TCP, TCP_KEEPIDLE, keepIdle, sizeof(keepIdle),
            keepIdleEc
        );
        if (!ec) {
            ec = keepIdleEc;
        }
#endif
#ifdef TCP_KEEPINTVL
        const auto keepIntvl = static_cast<uint32_t>(settings.KeepIntvl.count());
        std::error_code keepIntvlEc;
        SetSockOpt(
            socket.native_handle(), IPPROTO_TCP, TCP_KEEPINTVL, keepIntvl, sizeof(keepIntvl),
            keepIntvlEc
        );
        if (!ec) {
            ec = keepIntvlEc;
        }
#endif
    }
    return !ec;
}

} // namespace opendnp3
