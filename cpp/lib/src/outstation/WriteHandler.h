/*
 * Copyright 2013-2020 Automatak, LLC
 *
 * Licensed to Green Energy Corp (www.greenenergycorp.com) and Automatak
 * LLC (www.automatak.com) under one or more contributor license agreements.
 * See the NOTICE file distributed with this work for additional information
 * regarding copyright ownership. Green Energy Corp and Automatak LLC license
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
#ifndef OPENDNP3_WRITEHANDLER_H
#define OPENDNP3_WRITEHANDLER_H

#include "app/parsing/IAPDUHandler.h"
#include "outstation/TimeSyncState.h"

#include "opendnp3/app/IINField.h"
#include "opendnp3/logging/Logger.h"
#include "opendnp3/outstation/IOutstationApplication.h"
#include "opendnp3/util/Timestamp.h"

namespace opendnp3
{

class WriteHandler final : public IAPDUHandler
{
public:
    WriteHandler(IOutstationApplication& application,
                 TimeSyncState& timeSyncState,
                 AppSeqNum seq,
                 Timestamp now,
                 IINField* pWriteIIN);

    bool IsAllowed(uint32_t /*headerCount*/, GroupVariation gv, QualifierCode /*qc*/) override
    {
        if (gv == GroupVariation::Group70Var5)
        {
            isFileWrite = true;
        }
        return true;
    }

    bool IsFileWrite() const
    {
        return isFileWrite;
    }

private:
    IINField ProcessHeader(const RangeHeader& header, const ICollection<Indexed<IINValue>>& values) override;

    IINField ProcessHeader(const CountHeader& header, const ICollection<Group50Var1>& values) override;

    IINField ProcessHeader(const CountHeader& header, const ICollection<Group50Var3>& values) override;

    IINField ProcessHeader(const PrefixHeader& header,
                           const ICollection<Indexed<TimeAndInterval>>& values) override;

    IOutstationApplication* application;
    TimeSyncState* timeSyncState;
    AppSeqNum seq;
    Timestamp now;
    IINField* writeIIN;

    bool wroteTime = false;
    bool wroteIIN = false;
    bool isFileWrite = false; // if true - WRITE func used on file
};

} // namespace opendnp3

#endif
