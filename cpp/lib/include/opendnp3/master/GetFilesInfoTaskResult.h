#pragma once

#include "DNPFileInfo.h"
#include "opendnp3/gen/TaskCompletion.h"

#include <deque>
#include <functional>
#include <utility>

namespace opendnp3
{

class GetFilesInfoTaskResult
{
public:
    GetFilesInfoTaskResult(TaskCompletion summary)
        : summary(summary)
    { }

    GetFilesInfoTaskResult(TaskCompletion summary, std::deque<DNPFileInfo> filesInfo)
        : summary(summary), filesInfo(std::move(filesInfo))
    { }

    /// The result of the task as a whole.
    TaskCompletion summary{TaskCompletion::FAILURE_NO_COMMS};
    std::deque<DNPFileInfo> filesInfo;
};

using GetFilesInfoTaskCallbackT = std::function<void(const GetFilesInfoTaskResult&)>;

} // namespace opendnp3