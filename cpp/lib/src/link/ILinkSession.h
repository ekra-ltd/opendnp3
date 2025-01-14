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
#ifndef OPENDNP3_ILINKSESSION_H
#define OPENDNP3_ILINKSESSION_H

#include "IFrameSink.h"
#include "opendnp3/link/LinkStateChangeSource.h"

namespace opendnp3
{

// @section DESCRIPTION Interface from the link router to the link layer
class ILinkSession : public IFrameSink
{
public:
    virtual ~ILinkSession() {}

    // lower layer informs this layer that it is ready to transmit again
    virtual bool OnTxReady() = 0;

    virtual bool OnLowerLayerUp(LinkStateChangeSource source) = 0;

    virtual bool OnLowerLayerDown(LinkStateChangeSource source) = 0;

    virtual void OnResponseTimeout() = 0;
};

} // namespace opendnp3

#endif
