#include "DeleteFileTask.h"

#include "app/APDUBuilders.h"
#include "app/parsing/APDUParser.h"
#include "app/parsing/FileOperationHandler.h"
#include "gen/objects/Group70.h"
#include "logging/HexLogging.h"

#include <ser4cpp/serialization/LittleEndian.h>

#include <cstdint>
#include <utility>

namespace opendnp3
{

    DeleteFileTask::DeleteFileTask(
        const std::shared_ptr<TaskContext>& context,
        IMasterApplication& app,
        const Logger& logger,
        std::string filename,
        const TaskBehavior& taskBehavior,
        FileOperationTaskCallbackT taskCallback
    )
        : IMasterTask(context, app, taskBehavior, logger, TaskConfig::Default())
        , _filename(std::move(filename))
    {
        _callback = taskCallback ? std::move(taskCallback) : [](const FileOperationTaskResult& /**/) {};
    }

    void DeleteFileTask::Initialize()
    {
        _fileCommandStatus = Group70Var4();
    }

    bool DeleteFileTask::BuildRequest(APDURequest& request, uint8_t seq) {
        logger.log(flags::DBG, __FILE__, "Attempting delete file");
        Group70Var3 file;
        file.filename = _filename;
        file.operationMode = FileOpeningMode::DELETING;
        request.SetFunction(FunctionCode::DELETE_FILE);
        request.SetControl(AppControlField::Request(seq));
        auto writer = request.GetWriter();
        return writer.WriteSingleValue<ser4cpp::UInt8, Group70Var3>(QualifierCode::FREE_FORMAT, file);
    }

    IMasterTask::ResponseResult DeleteFileTask::ProcessResponse(const APDUResponseHeader& response,
                                                                const ser4cpp::rseq_t& objects) {
        if (ValidateSingleResponse(response)) {
            FileOperationHandler handler;
            const auto result = APDUParser::Parse(objects, handler, &logger);
            if (result != ParseResult::OK) {
                _callback(FileOperationTaskResult(TaskCompletion::FAILURE_BAD_RESPONSE));
                return ResponseResult::ERROR_BAD_RESPONSE;
            }
            _fileCommandStatus = handler.GetFileStatusObject();
            std::string s;
            switch (_fileCommandStatus.status) {
            case FileCommandStatus::SUCCESS:
                s = "Success deleting file - \"" + _filename + "\"";
                logger.log(flags::DBG, __FILE__, s.c_str());
                _callback(FileOperationTaskResult(TaskCompletion::SUCCESS));
                return ResponseResult::OK_FINAL;
            case FileCommandStatus::PERMISSION_DENIED:
                logger.log(flags::DBG, __FILE__, "Permission denied");
                break;
            case FileCommandStatus::INVALID_MODE:
                logger.log(flags::DBG, __FILE__, "Invalid mode");
                break;
            case FileCommandStatus::NOT_FOUND:
                s = "File - \"" + _filename + "\" not found";
                logger.log(flags::DBG, __FILE__, s.c_str());
                break;
            case FileCommandStatus::FILE_LOCKED:
                s = "File - \"" + _filename + "\" locked by another user";
                logger.log(flags::DBG, __FILE__, s.c_str());
                break;
            case FileCommandStatus::OPEN_COUNT_EXCEEDED:
                logger.log(flags::DBG, __FILE__, "Maximum amount of files opened");
                break;
            case FileCommandStatus::FILE_NOT_OPEN:
                s = "File - \"" + _filename + "\" not opened";
                logger.log(flags::DBG, __FILE__, s.c_str());
                break;
            case FileCommandStatus::INVALID_BLOCK_SIZE:
                logger.log(flags::DBG, __FILE__, "Cannot write this block size");
                break;
            case FileCommandStatus::LOST_COM:
                logger.log(flags::DBG, __FILE__, "Communication lost");
                break;
            case FileCommandStatus::FAILED_ABORT:
                logger.log(flags::DBG, __FILE__, "Abort action failed");
                break;
            default:
                logger.log(flags::DBG, __FILE__, "Unknown status code");
                break;
            }
        }

        _callback(FileOperationTaskResult(TaskCompletion::FAILURE_BAD_RESPONSE));
        return ResponseResult::ERROR_BAD_RESPONSE;
    }

} // namespace opendnp3
