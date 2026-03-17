#pragma once

#include "gen/objects/Group70.h"
#include "master/IMasterTask.h"
#include "master/TaskPriority.h"
#include "FileOperationTaskState.h"
#include "opendnp3/master/FileOperationTaskResult.h"


#include <sstream>
#include <string>

namespace opendnp3
{
    class ReadFileTask : public IMasterTask
    {

    public:
        ReadFileTask(
            const std::shared_ptr<TaskContext>& context,
            IMasterApplication& app,
            const Logger& logger,
            std::string sourceFilename,
            const TaskBehavior& taskBehavior,
            FileOperationTaskCallbackT taskCallback,
            uint16_t rxSize
        );

        char const* Name() const final
        {
            return "read file task";
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
            return MasterTaskType::READ_FILE_TASK;
        }

    private:
        bool IsEnabled() const final
        {
            return true;
        }

        ResponseResult ProcessResponse(const APDUResponseHeader& response,
            const ser4cpp::rseq_t& objects) final;

        ResponseResult OnResponseStatusObject(const APDUResponseHeader& response, const ser4cpp::rseq_t& objects);
        ResponseResult OnResponseReadFile(const APDUResponseHeader& header, const ser4cpp::rseq_t& objects);

        void Initialize() final;

    private:
        FileOperationTaskState _taskState{ OPENING };
        std::string _sourceFilename;
        std::ostringstream _outputFile;
        Group70Var4 _fileCommandStatus;
        Group70Var5 _fileTransportObject;
        FileOperationTaskCallbackT _callback;
        uint16_t _rxSize;
        bool _errorWhileReading{ false };
    };

} // namespace opendnp3