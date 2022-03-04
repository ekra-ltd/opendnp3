#pragma once

#include "gen/objects/Group70.h"
#include "master/IMasterTask.h"
#include "master/TaskPriority.h"
#include "FileOperationTaskState.h"
#include "opendnp3/master/FileOperationTaskResult.h"


#include <fstream>
#include <string>

namespace opendnp3
{
    class WriteFileTask : public IMasterTask
    {

    public:
        WriteFileTask(const std::shared_ptr<TaskContext>& context, IMasterApplication& app, const Logger& logger,
                      std::shared_ptr<std::ifstream> source, std::string destFilename, uint16_t txSize,
                      FileOperationTaskCallbackT taskCallback);

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
            return MasterTaskType::WRITE_FILE_TASK;
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

    private:
        FileOperationTaskState taskState{ OPENING };
        std::shared_ptr<std::ifstream> input_file;
        uint32_t inputFileSize;
        std::string destFilename;
        Group70Var4 fileCommandStatus;
        Group70Var5 fileTransportObject;
        Group70Var6 fileTransportStatusObject;
        FileOperationTaskCallbackT callback;
        uint16_t _txSize;
        bool errorWhileWriting = false;
    };

} // namespace opendnp3