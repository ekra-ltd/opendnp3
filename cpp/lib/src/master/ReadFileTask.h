#pragma once

#include "gen/objects/Group70.h"
#include "master/IMasterTask.h"
#include "master/TaskPriority.h"

#include <fstream>
#include <string>

namespace opendnp3
{
    enum ReadingTaskState
    {
        OPENING,
        READING,
        CLOSING
    };

    class ReadFileTask : public IMasterTask
    {

    public:
        ReadFileTask(const std::shared_ptr<TaskContext>& context, IMasterApplication& app, const Logger& logger,
                     std::string sourceFilename, const std::string& destFilename);

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
        ResponseResult OnResponseReadFile(const APDUResponseHeader& header, const ser4cpp::rseq_t& objects);

        void Initialize() final;

    protected:
        void OnTaskComplete(TaskCompletion result, Timestamp now) override;

    private:
        ReadingTaskState taskState{ OPENING };
        std::string sourceFilename;
        std::ofstream output_file;
        Group70Var4 fileCommandStatus;
        Group70Var5 fileTransportObject;
    };

} // namespace opendnp3