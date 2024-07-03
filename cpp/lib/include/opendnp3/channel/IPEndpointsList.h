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
#ifndef OPENDNP3_IPENDPOINTSLIST_H
#define OPENDNP3_IPENDPOINTSLIST_H

#include "opendnp3/channel/IPEndpoint.h"

#include <ostream>
#include <vector>

namespace opendnp3
{

class IPEndpointsList final
{
public:
    IPEndpointsList() = default;
    explicit IPEndpointsList(const std::vector<IPEndpoint>& endpoints);

    IPEndpointsList(const IPEndpointsList& rhs);
    IPEndpointsList& operator=(const IPEndpointsList& rhs);

    IPEndpointsList(IPEndpointsList&& rhs) noexcept;
    IPEndpointsList& operator=(IPEndpointsList&& other) noexcept;

    ~IPEndpointsList() = default;

    const IPEndpoint& GetCurrentEndpoint() const;
    bool Next();
    void Reset();

    friend bool operator==(const IPEndpointsList& lhs, const IPEndpointsList& rhs);
    friend bool operator!=(const IPEndpointsList& lhs, const IPEndpointsList& rhs);

    friend std::ostream& operator<<(std::ostream& os, const IPEndpointsList& obj);

private:
    std::vector<IPEndpoint> endpoints;
    std::vector<IPEndpoint>::const_iterator currentEndpoint;
};

} // namespace opendnp3

#endif
