#pragma once

#include "gen/objects/Group70.h"
#include "master/IMasterTask.h"
#include "master/TaskPriority.h"
#include "FileOperationTaskState.h"

#include <fstream>
#include <string>

namespace opendnp3
{
    class WriteFileTask : public IMasterTask
    {

    public:
        WriteFileTask(const std::shared_ptr<TaskContext>& context, IMasterApplication& app, const Logger& logger,
                     const std::string& sourceFilename, std::string destFilename);

        char const* Name() const final
        {
            return "write file task";
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
            return MasterTaskType::READ_FILE_TASK;
        }

        bool IsEnabled() const final
        {
            return true;
        }

        ResponseResult ProcessResponse(const APDUResponseHeader& response,
            const ser4cpp::rseq_t& objects) final;

        ResponseResult OnResponseStatusObject(const APDUResponseHeader& response, const ser4cpp::rseq_t& objects);
        ResponseResult OnResponseWriteFile(const APDUResponseHeader& header, const ser4cpp::rseq_t& objects);

        void Initialize() final;

    protected:
        void OnTaskComplete(TaskCompletion result, Timestamp now) override;

    private:
        FileOperationTaskState taskState{ OPENING };
        std::ifstream input_file;
        uint32_t inputFileSize;
        std::string destFilename;
        Group70Var4 fileCommandStatus;
        Group70Var5 fileTransportObject;
        Group70Var6 fileTransportStatusObject;
    };

} // namespace opendnp3