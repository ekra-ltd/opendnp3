#pragma once

#include "DNPFileInfo.h"
#include "opendnp3/gen/TaskCompletion.h"

#include <deque>
#include <functional>
#include <utility>

namespace opendnp3
{

class GetFilesInDirectoryTaskResult
{
public:
    GetFilesInDirectoryTaskResult(TaskCompletion summary)
        : summary(summary)
    { }

    GetFilesInDirectoryTaskResult(TaskCompletion summary, std::deque<DNPFileInfo> filesInfo)
        : summary(summary), filesInfo(std::move(filesInfo))
    { }

    /// The result of the task as a whole.
    TaskCompletion summary{TaskCompletion::FAILURE_NO_COMMS};
    std::deque<DNPFileInfo> filesInfo;
};

using GetFilesInDirectoryTaskCallbackT = std::function<void(const GetFilesInDirectoryTaskResult&)>;

} // namespace opendnp3