#pragma once

#include "gen/objects/Group70.h"
#include "master/IMasterTask.h"
#include "master/TaskPriority.h"
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

        MasterTaskType GetTaskType() const final
        {
            return MasterTaskType::GET_FILE_INFO_TASK;
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
        std::string sourceFile;
        DNPFileInfo fileInfo{};
        GetFilesInfoTaskCallbackT callback;
    };

} // namespace opendnp3