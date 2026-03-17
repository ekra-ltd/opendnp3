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
#include "MasterTasks.h"

#include "master/AssignClassTask.h"
#include "master/ClearRestartTask.h"
#include "master/DisableUnsolicitedTask.h"
#include "master/EnableUnsolicitedTask.h"
#include "master/EventScanTask.h"
#include "master/LANTimeSyncTask.h"
#include "master/SerialTimeSyncTask.h"
#include "master/StartupIntegrityPoll.h"

namespace opendnp3
{

MasterTasks::MasterTasks(
    const MasterParams& params,
    const Logger& logger,
    IMasterApplication& app,
    const std::shared_ptr<ISOEHandler>& SOEHandler
)
    : context(std::make_shared<TaskContext>())
    , clearRestart(std::make_shared<ClearRestartTask>(context, app, logger))
    , assignClass(std::make_shared<AssignClassTask>(context, app, RetryBehavior(params), logger))
    , startupIntegrity(std::make_shared<StartupIntegrityPoll>(
          context, app, SOEHandler, params.startupIntegrityClassMask, RetryBehavior(params), logger)
    )
    , eventScan(std::make_shared<EventScanTask>(
          context, app, SOEHandler, params.eventScanOnEventsAvailableClassMask, logger)
    )
    // optional tasks
    , disableUnsol(GetDisableUnsolTask(context, params, logger, app))
    , enableUnsol(GetEnableUnsolTask(context, params, logger, app))
    , timeSynchronization(GetTimeSyncTask(context, params, logger, app))
{}

void MasterTasks::Initialize(IMasterScheduler& scheduler, IMasterTaskRunner& runner) const
{
    for (auto& task :
         {clearRestart, assignClass, startupIntegrity, eventScan, enableUnsol, disableUnsol, timeSynchronization})
    {
        if (task)
            scheduler.Add(task, runner);
    }

    for (auto& task : boundTasks)
    {
        scheduler.Add(task, runner);
    }
}

void MasterTasks::BindTask(const std::shared_ptr<IMasterTask>& task)
{
    boundTasks.push_back(task);
}

bool MasterTasks::DemandTimeSync() const
{
    return demand(this->timeSynchronization);
}

bool MasterTasks::DemandEventScan() const
{
    return demand(this->eventScan);
}

bool MasterTasks::DemandIntegrity() const
{
    return demand(this->startupIntegrity);
}

void MasterTasks::OnRestartDetected() const
{
    demand(this->clearRestart);
    demand(this->assignClass);
    demand(this->startupIntegrity);
    demand(this->enableUnsol);
}

std::shared_ptr<IMasterTask> MasterTasks::GetTimeSyncTask(const std::shared_ptr<TaskContext>& context,
                                                          MasterParams params,
                                                          const Logger& logger,
                                                          IMasterApplication& application)
{
    const auto behavior = TaskBehavior::ImmediatePeriodic(
           params.timeSyncPeriod,
           params.taskRetryPeriod,
           params.maxTaskRetryPeriod,
           params.retryCount
    );
    switch (params.timeSyncMode)
    {
    case TimeSyncMode::NonLAN: {
        if (params.timeSyncPeriod == TimeDuration::Max()) {
            return std::make_shared<SerialTimeSyncTask>(context, application, logger);
        }
        return std::make_shared<SerialTimeSyncTask>(context, application, logger, behavior);
    }
    case TimeSyncMode::LAN: {
        if (params.timeSyncPeriod == TimeDuration::Max()) {
            return std::make_shared<LANTimeSyncTask>(context, application, logger);
        }
        return std::make_shared<LANTimeSyncTask>(context, application, logger, behavior);
    }
    default:
        return nullptr;
    }
}

std::shared_ptr<IMasterTask> MasterTasks::GetEnableUnsolTask(const std::shared_ptr<TaskContext>& context,
                                                             const MasterParams& params,
                                                             const Logger& logger,
                                                             IMasterApplication& application)
{
    return params.unsolClassMask.HasEventClass()
        ? std::make_shared<EnableUnsolicitedTask>(context, application, RetryBehavior(params), params.unsolClassMask,
                                                  logger)
        : nullptr;
}

std::shared_ptr<IMasterTask> MasterTasks::GetDisableUnsolTask(const std::shared_ptr<TaskContext>& context,
                                                              const MasterParams& params,
                                                              const Logger& logger,
                                                              IMasterApplication& application)
{
    return params.disableUnsolOnStartup
        ? std::make_shared<DisableUnsolicitedTask>(
              context, application,
              RetryBehavior(params),
              logger)
        : nullptr;
}

} // namespace opendnp3
