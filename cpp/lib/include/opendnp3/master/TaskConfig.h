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
#ifndef OPENDNP3_TASKCONFIG_H
#define OPENDNP3_TASKCONFIG_H

#include "opendnp3/master/ITaskCallback.h"
#include "opendnp3/master/TaskId.h"

#include <boost/optional/optional.hpp>

#include <memory>
#include <utility>

namespace opendnp3
{

/**
 *  Object containing multiple fields for configuring tasks
 */
class TaskConfig
{
public:
    TaskConfig(
        TaskId taskId,
        std::shared_ptr<ITaskCallback> pCallback,
        boost::optional<std::string> taskName = boost::none,
        bool canUseBackupChannel = true
    )
        : taskId(taskId)
        , pCallback(std::move(pCallback))
        , canUseBackupChannel(canUseBackupChannel)
        , taskName(std::move(taskName))
    {}

    static TaskConfig Default()
    {
        return { TaskId::Undefined(), nullptr, boost::none, true };
    }

    ///  --- syntax sugar for building configs -----

    static TaskConfig With(std::shared_ptr<ITaskCallback> callback, boost::optional<std::string> taskName = boost::none, bool canUseBackupChannel = true)
    {
        return { TaskId::Undefined(), std::move(callback), taskName, canUseBackupChannel };
    }

    TaskConfig() = delete;

public:
    TaskId taskId;
    std::shared_ptr<ITaskCallback> pCallback;
    bool canUseBackupChannel{ true };
    boost::optional<std::string> taskName;
};

} // namespace opendnp3

#endif
