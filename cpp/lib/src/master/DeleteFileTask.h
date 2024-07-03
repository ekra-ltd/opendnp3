#pragma once

#include "gen/objects/Group70.h"
#include "master/IMasterTask.h"
#include "master/TaskPriority.h"
#include "FileOperationTaskState.h"
#include "opendnp3/master/FileOperationTaskResult.h"

#include <string>

namespace opendnp3
{
    class DeleteFileTask : public IMasterTask
    {

    public:
        DeleteFileTask(const std::shared_ptr<TaskContext>& context, IMasterApplication& app, const Logger& logger,
            std::string filename, FileOperationTaskCallbackT taskCallback);

        char const* Name() const final
        {
            return "delete file task";
        }

        int Priority() const final
        {
            return priority::FILE_OPERATION;
        }

        bool BlocksLowerPriority() const final
        {
            return true;
        }

        bool IsRecurring() const final
        {
            return false;
        }

        bool BuildRequest(APDURequest& request, uint8_t seq) final;

        MasterTaskType GetTaskType() const final
        {
            return MasterTaskType::DELETE_FILE_TASK;
        }

    private:
        bool IsEnabled() const final
        {
            return true;
        }

        ResponseResult ProcessResponse(const APDUResponseHeader& response,
            const ser4cpp::rseq_t& objects) final;

        void Initialize() final;

    private:
        std::string filename;
        Group70Var4 fileCommandStatus;
        FileOperationTaskCallbackT callback;
    };

} // namespace opendnp3