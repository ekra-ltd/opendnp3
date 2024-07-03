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
#ifndef OPENDNP3_STARTUPINTEGRITYPOLL_H
#define OPENDNP3_STARTUPINTEGRITYPOLL_H

#include "master/PollTaskBase.h"
#include "master/TaskPriority.h"

#include "opendnp3/app/ClassField.h"

namespace opendnp3
{

class ISOEHandler;

/**
 * A generic interface for defining master request/response style tasks
 */
class StartupIntegrityPoll final : public PollTaskBase
{

public:
    StartupIntegrityPoll(const std::shared_ptr<TaskContext>& context,
                         IMasterApplication& app,
                         std::shared_ptr<ISOEHandler> soeHandler,
                         ClassField classes,
                         const TaskBehavior& behavior,
                         const Logger& logger);

    bool IsRecurring() const override
    {
        return true;
    }

    const char* Name() const override
    {
        return "Startup Integrity Poll";
    };

    bool BuildRequest(APDURequest& request, uint8_t seq) override;

    int Priority() const override
    {
        return priority::INTEGRITY_POLL;
    }

    bool BlocksLowerPriority() const override
    {
        return true;
    }

    MasterTaskType GetTaskType() const override
    {
        return MasterTaskType::STARTUP_INTEGRITY_POLL;
    }

private:
    ClassField classes;

    bool IsEnabled() const override;
};

} // namespace opendnp3

#endif
