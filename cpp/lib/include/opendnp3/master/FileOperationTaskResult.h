#pragma once

#include "opendnp3/gen/TaskCompletion.h"

#include <sstream>
#include <functional>
#include <utility>

namespace opendnp3
{

class FileOperationTaskResult
{
public:
    FileOperationTaskResult(TaskCompletion summary)
        : summary(summary)
    { }

    FileOperationTaskResult(TaskCompletion summary, std::ostringstream stream)
        : summary(summary), resultStream(std::move(stream))
    { }

    TaskCompletion summary{TaskCompletion::FAILURE_NO_COMMS};
    std::ostringstream resultStream;
};

using FileOperationTaskCallbackT = std::function<void(const FileOperationTaskResult&)>;

} // namespace opendnp3