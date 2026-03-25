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

#include "TaskBehavior.h"

namespace opendnp3
{

TaskBehavior TaskBehavior::SingleExecutionWithRetry(
    const Timestamp& startExpiration,
    const TimeDuration& minRetryDelay,
    const TimeDuration& maxRetryDelay,
    const NumRetries& retryCount
) {
    return {
        TimeDuration::Min(), // not periodic
        Timestamp::Min(),    // run immediately
        minRetryDelay,
        maxRetryDelay,
        startExpiration,
        retryCount
    };
}

TaskBehavior TaskBehavior::ImmediatePeriodic(
    const TimeDuration& period,
    const TimeDuration& minRetryDelay,
    const TimeDuration& maxRetryDelay,
    const NumRetries&   retryCount
) {
    return {
        period,
        Timestamp::Min(), // run immediately
        minRetryDelay,
        maxRetryDelay,
        Timestamp::Max(), // no start expiration
        retryCount
    };
}

TaskBehavior TaskBehavior::SingleImmediateExecutionWithRetry(
    const TimeDuration& minRetryDelay,
    const TimeDuration& maxRetryDelay,
    const NumRetries&   retryCount
) {
    return {
        TimeDuration::Min(), // not periodic
        Timestamp::Min(),    // run immediately
        minRetryDelay,
        maxRetryDelay,
        Timestamp::Max(),
        retryCount
    };
}

TaskBehavior TaskBehavior::ReactsToIINOnly()
{
    return {
        TimeDuration::Min(), // not periodic
        Timestamp::Max(),    // only run when needed
        TimeDuration::Max(), // never retry
        TimeDuration::Max(),
        Timestamp::Max(),
        NumRetries::Fixed(0)
    };
}

TaskBehavior::TaskBehavior(
    const TimeDuration& period,
    const Timestamp& expiration,
    const TimeDuration& minRetryDelay,
    const TimeDuration& maxRetryDelay,
    const Timestamp& startExpiration,
    const NumRetries& retryCount
)
    : period(period)
    , minRetryDelay(minRetryDelay)
    , maxRetryDelay(maxRetryDelay)
    , startExpiration(startExpiration)
    , expiration(expiration)
    , currentRetryDelay(minRetryDelay)
    , _retryCount(retryCount)
{
}

void TaskBehavior::OnSuccess(const Timestamp& now)
{
    _retryCount.Reset();
    this->currentRetryDelay = this->minRetryDelay;
    this->expiration = this->period.IsNegative() ? Timestamp::Max() : now + this->period;
}

TaskTimeoutStats TaskBehavior::OnResponseTimeout(const Timestamp& now)
{
    auto finished = false;
    TaskTimeoutStats stats{
        _retryCount.MaximumRetries(),
        _retryCount.CurrentRetry(),
        finished,
        _retryCount.IsFixed()
    };
    if (_retryCount.Retry()) {
        this->expiration = now + this->currentRetryDelay;
        this->currentRetryDelay = this->CalcNextRetryTimeout();
    }
    else {
        this->currentRetryDelay = this->minRetryDelay;
        this->expiration = this->period.IsNegative() ? Timestamp::Max() : now + this->period;
        _retryCount.Reset();
        finished = true;
    }
    stats.IsFinished = finished;
    return stats;
}

void TaskBehavior::Reset()
{
    this->disabled = false;
    this->expiration = Timestamp::Min();
    this->currentRetryDelay = this->minRetryDelay;
}

void TaskBehavior::Disable()
{
    this->disabled = true;
    this->expiration = Timestamp::Max();
}

void TaskBehavior::DelayByPeriod(const Timestamp& now)
{
    this->currentRetryDelay = this->minRetryDelay;
    this->expiration = this->period.IsNegative() ? Timestamp::Max() : now + this->period;
    _retryCount.Reset();
}

TimeDuration TaskBehavior::CalcNextRetryTimeout() const
{
    if (_retryCount.IsFixed()) {
        return this->minRetryDelay;
    }
    const auto doubled = this->currentRetryDelay.Double();
    return doubled > this->maxRetryDelay ? this->maxRetryDelay : doubled;
}

} // namespace opendnp3
