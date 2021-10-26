#include "GetFileInfoTask.h"

#include "app/APDUBuilders.h"
#include "app/parsing/APDUParser.h"
#include "gen/objects/Group70.h"
#include "logging/HexLogging.h"
#include "master/FileOperationHandler.h"

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
        sourceFile(std::move(sourceFile)),
        callback(std::move(taskCallback))
    { }

    void GetFileInfoTask::Initialize()
    {
        currentTaskState = GETTING_FILE_INFO;
        fileCommandStatus = Group70Var4();
        fileInfo = DNPFileInfo();
    }

    bool GetFileInfoTask::BuildRequest(APDURequest& request, uint8_t seq) {
        switch (currentTaskState) {
            case OPENING: {
                logger.log(flags::DBG, __FILE__, "Attempting opening file");
                Group70Var3 file;
                file.filename = sourceFile;
                request.SetFunction(FunctionCode::OPEN_FILE);
                request.SetControl(AppControlField::Request(seq));
                auto writer = request.GetWriter();
                return writer.WriteSingleValue<ser4cpp::UInt8, Group70Var3>(QualifierCode::FREE_FORMAT, file);
            }
            case GETTING_FILE_INFO: {
                Group70Var7 obj;
                obj.fileInfo.filename = sourceFile;
                obj.requestId = fileCommandStatus.requestId;
                request.SetFunction(FunctionCode::GET_FILE_INFO);
                request.SetControl(AppControlField::Request(seq));
                auto writer = request.GetWriter();
                return writer.WriteSingleValue<ser4cpp::UInt8, Group70Var7>(QualifierCode::FREE_FORMAT, obj);
            }
            case CLOSING: {
                request.SetFunction(FunctionCode::CLOSE_FILE);
                request.SetControl(AppControlField::Request(seq));
                auto writer = request.GetWriter();
                return writer.WriteSingleValue<ser4cpp::UInt8, Group70Var4>(QualifierCode::FREE_FORMAT, fileCommandStatus);
            }
        }

        return false;
    }

    IMasterTask::ResponseResult GetFileInfoTask::ProcessResponse(const APDUResponseHeader& response,
        const ser4cpp::rseq_t& objects) {
        switch (currentTaskState) {
        case OPENING:
        case CLOSING:
            return OnResponseStatusObject(response, objects);
        case GETTING_FILE_INFO:
            return OnResponseGetInfo(response, objects);
        default:
            return ResponseResult::ERROR_BAD_RESPONSE;
        }
    }

    IMasterTask::ResponseResult GetFileInfoTask::HandleCommandStatus()
    {
        std::string s;
        switch (fileCommandStatus.status) {
        case FileCommandStatus::SUCCESS: {
            switch (currentTaskState) {
            case OPENING:
                s = "Opened file - \"" + sourceFile + "\"";
                logger.log(flags::DBG, __FILE__, s.c_str());
                currentTaskState = GETTING_FILE_INFO;
                return ResponseResult::OK_REPEAT;
            case GETTING_FILE_INFO:
                logger.log(flags::DBG, __FILE__, "Successfully received file info");
                currentTaskState = CLOSING;
                return ResponseResult::OK_REPEAT;
            case CLOSING:
                s = "Successfully closed file - \"" + sourceFile + "\"";
                logger.log(flags::DBG, __FILE__, s.c_str());
                callback(GetFilesInfoTaskResult(TaskCompletion::SUCCESS, { fileInfo }));
                return ResponseResult::OK_FINAL;
            default:
                return ResponseResult::ERROR_BAD_RESPONSE;
            }
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

    IMasterTask::ResponseResult GetFileInfoTask::OnResponseStatusObject(const APDUResponseHeader& response,
                                                                        const ser4cpp::rseq_t& objects) {
        if (ValidateSingleResponse(response)) {
            FileOperationHandler handler;
            const auto result = APDUParser::Parse(objects, handler, &logger);
            if (result != ParseResult::OK) {
                return ResponseResult::ERROR_BAD_RESPONSE;
            }
            fileCommandStatus = handler.GetFileStatusObject();
            return HandleCommandStatus();
        }

        callback(GetFilesInfoTaskResult(TaskCompletion::FAILURE_BAD_RESPONSE));
        return ResponseResult::ERROR_BAD_RESPONSE;
    }

    IMasterTask::ResponseResult GetFileInfoTask::OnResponseGetInfo(const APDUResponseHeader& header,
                                                                   const ser4cpp::rseq_t& objects) {
        if (ValidateSingleResponse(header)) {
            FileOperationHandler handler;
            const auto result = APDUParser::Parse(objects, handler, &logger);
            if (result != ParseResult::OK) {
                callback(GetFilesInfoTaskResult(TaskCompletion::FAILURE_BAD_RESPONSE));
                return ResponseResult::ERROR_BAD_RESPONSE;
            }

            const Group70Var7 obj = handler.GetFileDescriptorObject();
            if (obj.isInitialized) {
                fileInfo = obj.fileInfo;
            }
            else {
                fileCommandStatus = handler.GetFileStatusObject();
                return HandleCommandStatus();
            }
            currentTaskState = CLOSING;
            return ResponseResult::OK_REPEAT;
        }
        return ResponseResult::ERROR_BAD_RESPONSE;
    }

} // namespace opendnp3
