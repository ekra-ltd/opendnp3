#pragma once

#include "gen/objects/Group70.h"
#include "master/IMasterTask.h"
#include "master/TaskPriority.h"
#include "FileOperationTaskState.h"
#include "opendnp3/master/GetFilesInfoTaskResult.h"

#include <string>

namespace opendnp3
{
    class GetFileInfoTask : public IMasterTask {

    public:
        GetFileInfoTask(const std::shared_ptr<TaskContext>& context, IMasterApplication& app, const Logger& logger,
            std::string sourceFile, GetFilesInfoTaskCallbackT taskCallback);

        char const* Name() const final
        {
            return "get file info task";
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

    private:
        MasterTaskType GetTaskType() const final
        {
            return MasterTaskType::GET_FILE_INFO_TASK;
        }

        bool IsEnabled() const final
        {
            return true;
        }

        ResponseResult ProcessResponse(const APDUResponseHeader& response,
            const ser4cpp::rseq_t& objects) final;

        ResponseResult OnResponseStatusObject(const APDUResponseHeader& response, const ser4cpp::rseq_t& objects);
        ResponseResult OnResponseGetInfo(const APDUResponseHeader& header, const ser4cpp::rseq_t& objects);

        void Initialize() final;

        ResponseResult HandleCommandStatus();

    private:
        FileOperationTaskState currentTaskState{ OPENING };
        std::string sourceFile;
        Group70Var4 fileCommandStatus;
        DNPFileInfo fileInfo;
        GetFilesInfoTaskCallbackT callback;
    };

} // namespace opendnp3