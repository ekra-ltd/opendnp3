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
#ifndef OPENDNP3_ITASKCALLBACK_H
#define OPENDNP3_ITASKCALLBACK_H

#include "opendnp3/gen/TaskCompletion.h"

namespace opendnp3
{

/**
 * Callbacks for when a task starts and completes
 */
class ITaskCallback
{
public:
    // Called when the task starts running
    virtual void OnStart(const std::string& taskName) = 0;

    // Called when the task succeeds or fails
    virtual void OnComplete(TaskCompletion result, const std::string& taskName) = 0;

    // Called when the task no longer exists and no more calls will be made to OnStart/OnComplete
    virtual void OnDestroyed() = 0;
};

} // namespace opendnp3

#endif
