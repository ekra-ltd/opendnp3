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
#ifndef OPENDNP3_DECODER_H
#define OPENDNP3_DECODER_H

#include "opendnp3/StatisticsTypes.h"
#include "opendnp3/decoder/IDecoderCallbacks.h"
#include "opendnp3/logging/Logger.h"
#include "opendnp3/util/Buffer.h"

namespace opendnp3
{

class DecoderImpl;

// stand-alone DNP3 decoder
class Decoder
{
public:
    Decoder(IDecoderCallbacks& callbacks, const Logger& logger);
    ~Decoder();

    void DecodeLPDU(const Buffer& data);
    void DecodeTPDU(const Buffer& data);
    void DecodeAPDU(const Buffer& data);

private:
    DecoderImpl* impl;
};

} // namespace opendnp3

#endif
