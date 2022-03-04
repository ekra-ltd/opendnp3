#include "GetFileInfoTask.h"

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

    GetFileInfoTask::GetFileInfoTask(const std::shared_ptr<TaskContext>& context,
        IMasterApplication& app,
        const Logger& logger,
        std::string sourceFile,
        GetFilesInfoTaskCallbackT taskCallback)
        : IMasterTask(context, app, TaskBehavior::SingleExecutionNoRetry(), logger, TaskConfig::Default()),
          sourceFile(std::move(sourceFile)), callback(std::move(taskCallback))
    { }

    void GetFileInfoTask::Initialize()
    {
        fileInfo = DNPFileInfo();
        if (callback == nullptr) {
            callback = [](const GetFilesInfoTaskResult& /**/) {};
        }
    }

    bool GetFileInfoTask::BuildRequest(APDURequest& request, uint8_t seq) {
        Group70Var7 obj;
        obj.fileInfo.filename = sourceFile;
        obj.asData = false; // obj isn't a part of another object
        request.SetFunction(FunctionCode::GET_FILE_INFO);
        request.SetControl(AppControlField::Request(seq));
        auto writer = request.GetWriter();
        return writer.WriteSingleValue<ser4cpp::UInt8, Group70Var7>(QualifierCode::FREE_FORMAT, obj);
    }

    IMasterTask::ResponseResult GetFileInfoTask::ProcessResponse(const APDUResponseHeader& response, const ser4cpp::rseq_t& objects) {
        if (ValidateSingleResponse(response)) {
            FileOperationHandler handler;
            const auto result = APDUParser::Parse(objects, handler, &logger);
            if (result != ParseResult::OK) {
                return ResponseResult::ERROR_BAD_RESPONSE;
            }

            // we can get either var 7 or var 4 as a response
            const Group70Var7 obj = handler.GetFileDescriptorObject();
            if (obj.isInitialized) { // if obj is inialized - response contains var 7
                fileInfo = obj.fileInfo;
                callback(GetFilesInfoTaskResult(TaskCompletion::SUCCESS, { fileInfo }));
                return ResponseResult::OK_FINAL;
            }
            const auto fileCommandStatus = handler.GetFileStatusObject();
            if (!fileCommandStatus.isInitialize) // both var4 and var7 not received
            {
                return ResponseResult::ERROR_BAD_RESPONSE;
            }
            std::string s;
            switch (fileCommandStatus.status) {
            case FileCommandStatus::SUCCESS: {
                logger.log(flags::DBG, __FILE__, "Successfully received file info");
                callback(GetFilesInfoTaskResult(TaskCompletion::SUCCESS, { fileInfo }));
                return ResponseResult::OK_FINAL;
            }
            case FileCommandStatus::PERMISSION_DENIED:
                logger.log(flags::DBG, __FILE__, "Permission denied");
                break;
            case FileCommandStatus::INVALID_MODE:
                logger.log(flags::DBG, __FILE__, "Invalid mode");
                break;
            case FileCommandStatus::NOT_FOUND:
                s = "File - \"" + sourceFile + "\" not found";
                logger.log(flags::DBG, __FILE__, s.c_str());
                break;
            case FileCommandStatus::FILE_LOCKED:
                s = "File - \"" + sourceFile + "\" locked by another user";
                logger.log(flags::DBG, __FILE__, s.c_str());
                break;
            case FileCommandStatus::OPEN_COUNT_EXCEEDED:
                logger.log(flags::DBG, __FILE__, "Maximum amount of files opened");
                break;
            case FileCommandStatus::FILE_NOT_OPEN:
                s = "File - \"" + sourceFile + "\" not opened";
                logger.log(flags::DBG, __FILE__, s.c_str());
                break;
            case FileCommandStatus::INVALID_BLOCK_SIZE:
                logger.log(flags::DBG, __FILE__, "Cannot write this block size");
                break;
            case FileCommandStatus::LOST_COM:
                logger.log(flags::DBG, __FILE__, "Communication lost");
                callback(GetFilesInfoTaskResult(TaskCompletion::FAILURE_NO_COMMS));
                break;
            case FileCommandStatus::FAILED_ABORT:
                logger.log(flags::DBG, __FILE__, "Abort action failed");
                break;
            default:
                logger.log(flags::DBG, __FILE__, "Unknown status code");
                break;
            }
            return ResponseResult::ERROR_BAD_RESPONSE;
        }
        return ResponseResult::ERROR_BAD_RESPONSE;
    }

} // namespace opendnp3
