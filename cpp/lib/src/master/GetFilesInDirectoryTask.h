#pragma once

#include "gen/objects/Group70.h"
#include "master/IMasterTask.h"
#include "master/TaskPriority.h"
#include "FileOperationTaskState.h"
#include "opendnp3/master/GetFilesInfoTaskResult.h"


#include <deque>
#include <string>

namespace opendnp3
{
    class GetFilesInDirectoryTask : public IMasterTask {

    public:
        GetFilesInDirectoryTask(const std::shared_ptr<TaskContext>& context, IMasterApplication& app, const Logger& logger,
            std::string sourceDirectory, GetFilesInfoTaskCallbackT taskCallback, uint16_t rxSize);

        char const* Name() const final
        {
            return "get files in directory task";
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
            return MasterTaskType::GET_DIRECTORY_FILES_TASK;
        }

        bool IsEnabled() const final
        {
            return true;
        }

        ResponseResult ProcessResponse(const APDUResponseHeader& response,
            const ser4cpp::rseq_t& objects) final;

        ResponseResult OnResponseStatusObject(const APDUResponseHeader& response, const ser4cpp::rseq_t& objects);
        ResponseResult OnResponseReadDirectory(const APDUResponseHeader& header, const ser4cpp::rseq_t& objects);

        void Initialize() final;

    private:
        FileOperationTaskState currentTaskState{ OPENING };
        std::string sourceDirectory;
        Group70Var4 fileCommandStatus;
        Group70Var5 fileTransportObject;
        std::deque<DNPFileInfo> filesInfo;
        GetFilesInfoTaskCallbackT callback;
        uint16_t _rxSize;
        bool errorWhileReading = false;
    };

} // namespace opendnp3