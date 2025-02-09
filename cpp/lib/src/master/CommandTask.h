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
#ifndef OPENDNP3_COMMANDTASK_H
#define OPENDNP3_COMMANDTASK_H

#include "master/IMasterTask.h"
#include "master/TaskPriority.h"

#include "opendnp3/gen/FunctionCode.h"
#include "opendnp3/gen/IndexQualifierMode.h"
#include "opendnp3/logging/Logger.h"
#include "opendnp3/master/CommandSet.h"
#include "opendnp3/master/ICommandProcessor.h"
#include "opendnp3/master/ITaskCallback.h"

#include <assert.h>

#include <deque>
#include <memory>

namespace opendnp3
{

// Base class with machinery for performing command operations
class CommandTask : public IMasterTask
{

public:
    CommandTask(const std::shared_ptr<TaskContext>& context,
                CommandSet&& commands,
                IndexQualifierMode mode,
                IMasterApplication& app,
                CommandResultCallbackT callback,
                const Timestamp& startExpiration,
                const TaskConfig& config,
                const Logger& logger);

    static std::shared_ptr<IMasterTask> CreateDirectOperate(const std::shared_ptr<TaskContext>& context,
                                                            CommandSet&& set,
                                                            IndexQualifierMode mode,
                                                            IMasterApplication& app,
                                                            const CommandResultCallbackT& callback,
                                                            const Timestamp& startExpiration,
                                                            const TaskConfig& config,
                                                            Logger logger);
    static std::shared_ptr<IMasterTask> CreateSelectAndOperate(const std::shared_ptr<TaskContext>& context,
                                                               CommandSet&& set,
                                                               IndexQualifierMode mode,
                                                               IMasterApplication& app,
                                                               const CommandResultCallbackT& callback,
                                                               const Timestamp& startExpiration,
                                                               const TaskConfig& config,
                                                               Logger logger);
    static std::shared_ptr<IMasterTask> CreateSelect(const std::shared_ptr<TaskContext>& context,
                                                     CommandSet&& set,
                                                     IndexQualifierMode mode,
                                                     IMasterApplication& app,
                                                     const CommandResultCallbackT& callback,
                                                     const Timestamp& startExpiration,
                                                     const TaskConfig& config,
                                                     Logger logger);

    static std::shared_ptr<IMasterTask> CreateOperate(const std::shared_ptr<TaskContext>& context,
                                                      CommandSet&& set,
                                                      IndexQualifierMode mode,
                                                      IMasterApplication& app,
                                                      const CommandResultCallbackT& callback,
                                                      const Timestamp& startExpiration,
                                                      const TaskConfig& config,
                                                      Logger logger);


    char const* Name() const final
    {
        return "Command Task";
    }

    int Priority() const final
    {
        return priority::COMMAND;
    }

    bool BlocksLowerPriority() const final
    {
        return false;
    }

    bool IsRecurring() const final
    {
        return false;
    }

    bool BuildRequest(APDURequest& request, uint8_t seq) final;

    MasterTaskType GetTaskType() const final
    {
        return MasterTaskType::USER_TASK;
    }

private:
    bool IsEnabled() const final
    {
        return true;
    }

    void Initialize() final;

    ResponseResult ProcessResponse(const APDUResponseHeader& header,
                                   const ser4cpp::rseq_t& objects) final;

    void OnTaskComplete(TaskCompletion result, Timestamp now) final;

    ResponseResult ProcessResponse(const ser4cpp::rseq_t& objects);

    void LoadSelectAndOperate();
    void LoadDirectOperate();
    void LoadSelect();
    void LoadOperate();

    std::deque<FunctionCode> functionCodes;

    CommandStatus statusResult;
    const CommandResultCallbackT commandCallback;
    CommandSet commands;
    const IndexQualifierMode mode;

    bool isSelect = false;
};

} // namespace opendnp3

#endif
