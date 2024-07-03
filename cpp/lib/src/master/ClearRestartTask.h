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
#ifndef OPENDNP3_CLEARRESTARTTASK_H
#define OPENDNP3_CLEARRESTARTTASK_H

#include "master/IMasterTask.h"
#include "master/TaskPriority.h"

namespace opendnp3
{

/**
 * Clear the IIN restart bit
 */
class ClearRestartTask final : public IMasterTask
{

public:
    ClearRestartTask(const std::shared_ptr<TaskContext>& context,
                     IMasterApplication& application,
                     const Logger& logger);

    char const* Name() const override
    {
        return "Clear Restart IIN";
    }

    bool IsRecurring() const override
    {
        return true;
    }

    int Priority() const override
    {
        return priority::CLEAR_RESTART;
    }

    bool BlocksLowerPriority() const override
    {
        return true;
    }

    bool BuildRequest(APDURequest& request, uint8_t seq) override;

    MasterTaskType GetTaskType() const override
    {
        return MasterTaskType::CLEAR_RESTART;
    }

private:
    bool IsEnabled() const override
    {
        return true;
    }

    ResponseResult ProcessResponse(const APDUResponseHeader& response, const ser4cpp::rseq_t& objects) override;
};

} // namespace opendnp3

#endif
