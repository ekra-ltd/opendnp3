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
#include "opendnp3/channel/IPEndpointsList.h"

#include <cassert>

namespace opendnp3
{

IPEndpointsList::IPEndpointsList(const std::vector<IPEndpoint>& endpoints)
    : endpoints(endpoints)
    , currentEndpoint(this->endpoints.begin())
{
    assert(!endpoints.empty());
}

IPEndpointsList::IPEndpointsList(const IPEndpointsList& rhs)
    : endpoints(rhs.endpoints), currentEndpoint(this->endpoints.begin())
{
}

IPEndpointsList::IPEndpointsList(IPEndpointsList&& rhs) noexcept
    : endpoints(std::move(rhs.endpoints))
    , currentEndpoint(rhs.currentEndpoint)
{}

IPEndpointsList& IPEndpointsList::operator=(const IPEndpointsList& rhs)
{
    if (this == &rhs)
    {
        return *this;
    }
    endpoints = rhs.endpoints;
    currentEndpoint = this->endpoints.begin();;
    return *this;
}

IPEndpointsList& IPEndpointsList::operator=(IPEndpointsList&& other) noexcept
{
    if (this == &other)
    {
        return *this;
    }
    endpoints = std::move(other.endpoints);
    currentEndpoint = other.currentEndpoint;
    return *this;
}

const IPEndpoint& IPEndpointsList::GetCurrentEndpoint() const
{
    return *this->currentEndpoint;
}

bool IPEndpointsList::Next()
{
    auto result = true;
    ++this->currentEndpoint;
    if (this->currentEndpoint == this->endpoints.end())
    {
        Reset();
        result = false;
    }
    return result;
}

void IPEndpointsList::Reset()
{
    this->currentEndpoint = this->endpoints.begin();
}

bool operator==(const IPEndpointsList& lhs, const IPEndpointsList& rhs)
{
    return lhs.endpoints == rhs.endpoints
        && lhs.currentEndpoint == rhs.currentEndpoint;
}

bool operator!=(const IPEndpointsList& lhs, const IPEndpointsList& rhs)
{
    return !(lhs == rhs);
}

std::ostream& operator<<(std::ostream& os, const IPEndpointsList& obj)
{
    os << "endpoints: " << std::endl;
    for (const auto& endpoint : obj.endpoints)
    {
        os << "\t" << endpoint;
    }
    os << std::endl << "\tcurrentEndpoint: " << *obj.currentEndpoint << std::endl;
    return os;
}

} // namespace opendnp3
