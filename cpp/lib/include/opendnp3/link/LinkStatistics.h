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
#ifndef OPENDNP3_LINKSTATISTICS_H
#define OPENDNP3_LINKSTATISTICS_H

#include "opendnp3/StatisticsTypes.h"

namespace opendnp3
{

/**
 * Counters for the channel and the DNP3 link layer
 */
struct LinkStatistics
{
    struct Parser
    {
        Parser()
        {
            auto handler = [&](StatisticsValueType type, uint64_t value) {
                if (changeHandler) {
                    changeHandler(type, value);
                }
            };

            numHeaderCrcError  = { 0, StatisticsValueType::ChecksumErrors,    handler };
            numBodyCrcError    = { 0, StatisticsValueType::ChecksumErrors,    handler };
            numLinkFrameRx     = { 0, StatisticsValueType::FramesReceived,    handler };
            numBadLength       = { 0, StatisticsValueType::FrameFormatErrors, handler };
            numBadFunctionCode = { 0, StatisticsValueType::FrameFormatErrors, handler };
            numBadFCV          = { 0, StatisticsValueType::FrameFormatErrors, handler };
            numBadFCB          = { 0, StatisticsValueType::FrameFormatErrors, handler };
        }

        StatisticsChangeHandler_t changeHandler = nullptr;

        /// Number of frames discarded due to header CRC errors
        StatisticValueWithEvent numHeaderCrcError;

        /// Number of frames discarded due to body CRC errors
        StatisticValueWithEvent numBodyCrcError;

        /// Number of frames received
        StatisticValueWithEvent numLinkFrameRx;

        /// number of bad LEN fields received (malformed frame)
        StatisticValueWithEvent numBadLength;

        /// number of bad function codes (malformed frame)
        StatisticValueWithEvent numBadFunctionCode;

        /// number of FCV / function code mismatches (malformed frame)
        StatisticValueWithEvent numBadFCV;

        /// number of frames w/ unexpected FCB bit set (malformed frame)
        StatisticValueWithEvent numBadFCB;
    };

    struct Channel
    {
        Channel()
        {
            auto handler = [&](StatisticsValueType type, uint64_t value) {
                if (changeHandler) {
                    changeHandler(type, value);
                }
            };
            numOpen        = { 0, StatisticsValueType::SucceededConnections, handler };
            numOpenFail    = { 0, StatisticsValueType::FailedConnections,    handler };
            numClose       = { 0, StatisticsValueType::LostConnections,      handler };
            numBytesRx     = { 0, StatisticsValueType::BytesReceived,        handler };
            numBytesTx     = { 0, StatisticsValueType::BytesSent,            handler };
            numLinkFrameTx = { 0, StatisticsValueType::FramesSent,           handler };
        }

        StatisticsChangeHandler_t changeHandler = nullptr;

        /// The number of times the channel has successfully opened
        StatisticValueWithEvent numOpen;

        /// The number of times the channel has failed to open
        StatisticValueWithEvent numOpenFail;

        /// The number of times the channel has closed either due to user intervention or an error
        StatisticValueWithEvent numClose;

        /// The number of bytes received
        StatisticValueWithEvent numBytesRx;

        /// The number of bytes transmitted
        StatisticValueWithEvent numBytesTx;

        /// Number of frames transmitted
        StatisticValueWithEvent numLinkFrameTx;
    };

    LinkStatistics() = default;

    LinkStatistics(const Channel& channel, const Parser& parser) : channel(channel), parser(parser) {}

    /// statistics for the communicaiton channel
    Channel channel;

    /// statistics for the link parser
    Parser parser;
};

} // namespace opendnp3

#endif
