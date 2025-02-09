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
#include "LinkLayer.h"

namespace opendnp3
{

LinkLayer::LinkLayer(const Logger& logger,
                     const std::shared_ptr<exe4cpp::IExecutor>& executor,
                     const std::shared_ptr<IUpperLayer>& upper,
                     const std::shared_ptr<ILinkListener>& listener,
                     const LinkLayerConfig& config)
    : ctx(LinkContext::Create(logger, executor, upper, listener, *this, config))
{
}

const StackStatistics::Link& LinkLayer::GetStatistics() const
{
    return this->ctx->statistics;
}

void LinkLayer::SetRouter(ILinkTx& router)
{
    assert(!ctx->linktx);
    ctx->linktx = &router;
}

////////////////////////////////
// ILowerLayer
////////////////////////////////

bool LinkLayer::Send(ITransportSegment& segments)
{
    if (!ctx->isOnline)
        return false;

    if (ctx->SetTxSegment(segments))
    {
        ctx->TryStartTransmission();
    }

    return true;
}

////////////////////////////////
// ILinkSession
////////////////////////////////

bool LinkLayer::OnLowerLayerUp(LinkStateChangeSource source)
{
    return ctx->OnLowerLayerUp(source);
}

bool LinkLayer::OnLowerLayerDown(LinkStateChangeSource source)
{
    return ctx->OnLowerLayerDown(source);
}

bool LinkLayer::OnTxReady()
{
    auto ret = ctx->OnTxReady();

    if (ret)
    {
        ctx->TryStartTransmission();
    }

    return true;
}

////////////////////////////////
// IFrameSink
////////////////////////////////

bool LinkLayer::OnFrame(const LinkHeaderFields& header, const ser4cpp::rseq_t& userdata)
{
    auto ret = this->ctx->OnFrame(header, userdata);

    if (ret)
    {
        this->ctx->TryStartTransmission();
    }

    return ret;
}

void LinkLayer::OnResponseTimeout()
{
    // do nothing
}

} // namespace opendnp3
