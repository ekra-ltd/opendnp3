#include "GetFilesInDirectoryTask.h"

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

    GetFilesInDirectoryTask::GetFilesInDirectoryTask(const std::shared_ptr<TaskContext>& context,
        IMasterApplication& app,
        const Logger& logger,
        std::string sourceDirectory,
        GetFilesInfoTaskCallbackT taskCallback)
        : IMasterTask(context, app, TaskBehavior::SingleExecutionNoRetry(), logger, TaskConfig::Default()),
        sourceDirectory(std::move(sourceDirectory)),
        callback(std::move(taskCallback))
    { }

    void GetFilesInDirectoryTask::Initialize()
    {
        currentTaskState = OPENING;
        fileCommandStatus = Group70Var4();
        fileTransportObject = Group70Var5();
        filesInfo.clear();
        if (callback == nullptr) {
            callback = [](const GetFilesInfoTaskResult& /**/) {};
        }
    }

    bool GetFilesInDirectoryTask::BuildRequest(APDURequest& request, uint8_t seq) {
        switch (currentTaskState) {
            case OPENING: {
                logger.log(flags::DBG, __FILE__, "Attempting opening directory");
                Group70Var3 file;
                file.filename = sourceDirectory;
                request.SetFunction(FunctionCode::OPEN_FILE);
                request.SetControl(AppControlField::Request(seq));
                auto writer = request.GetWriter();
                return writer.WriteSingleValue<ser4cpp::UInt8, Group70Var3>(QualifierCode::FREE_FORMAT, file);
            }
            case READING_DIRECTORY: {
                fileTransportObject.fileId = fileCommandStatus.fileId;
                request.SetFunction(FunctionCode::READ);
                request.SetControl(AppControlField::Request(seq));
                auto writer = request.GetWriter();
                return writer.WriteSingleValue<ser4cpp::UInt8, Group70Var5>(QualifierCode::FREE_FORMAT, fileTransportObject);
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

    IMasterTask::ResponseResult GetFilesInDirectoryTask::ProcessResponse(const APDUResponseHeader& response,
                                                              const ser4cpp::rseq_t& objects) {
        switch (currentTaskState) {
            case OPENING:
            case CLOSING:
                return OnResponseStatusObject(response, objects);
            case READING_DIRECTORY:
                return OnResponseReadDirectory(response, objects);
            default:
                return ResponseResult::ERROR_BAD_RESPONSE;
        }
    }

    IMasterTask::ResponseResult GetFilesInDirectoryTask::OnResponseStatusObject(const APDUResponseHeader& response,
                                                                 const ser4cpp::rseq_t& objects) {
        if (ValidateSingleResponse(response)) {
            FileOperationHandler handler;
            const auto result = APDUParser::Parse(objects, handler, &logger);
            if (result != ParseResult::OK) {
                return ResponseResult::ERROR_BAD_RESPONSE;
            }
            fileCommandStatus = handler.GetFileStatusObject();
            std::string s;
            switch (fileCommandStatus.status) {
                case FileCommandStatus::SUCCESS: {
                    switch (currentTaskState) {
                        case OPENING:
                            s = "Reading files in directory - \"" + sourceDirectory + "\"";
                            logger.log(flags::DBG, __FILE__, s.c_str());
                            currentTaskState = READING_DIRECTORY;
                            return ResponseResult::OK_REPEAT;
                        case READING_DIRECTORY:
                            logger.log(flags::DBG, __FILE__, "Successfully received file names");
                            logger.log(flags::DBG, __FILE__, "Reading each file...");
                            currentTaskState = CLOSING;
                            return ResponseResult::OK_REPEAT;
                        case CLOSING:
                            s = "Successfully closed directory - \"" + sourceDirectory + "\"";
                            logger.log(flags::DBG, __FILE__, s.c_str());
                            callback(GetFilesInfoTaskResult(TaskCompletion::SUCCESS, filesInfo));
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
                    s = "Directory - \"" + sourceDirectory + "\" not found";
                    logger.log(flags::DBG, __FILE__, s.c_str());
                    break;
                case FileCommandStatus::FILE_LOCKED:
                    s = "Directory - \"" + sourceDirectory + "\" locked by another user";
                    logger.log(flags::DBG, __FILE__, s.c_str());
                    break;
                case FileCommandStatus::OPEN_COUNT_EXCEEDED:
                    logger.log(flags::DBG, __FILE__, "Maximum amount of files opened");
                    break;
                case FileCommandStatus::FILE_NOT_OPEN:
                    s = "Directory - \"" + sourceDirectory + "\" not opened";
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
        }

        callback(GetFilesInfoTaskResult(TaskCompletion::FAILURE_BAD_RESPONSE));
        return ResponseResult::ERROR_BAD_RESPONSE;
    }

    IMasterTask::ResponseResult GetFilesInDirectoryTask::OnResponseReadDirectory(const APDUResponseHeader& header,
                                                                           const ser4cpp::rseq_t& objects) {
        if (ValidateSingleResponse(header)) {
            FileOperationHandler handler;
            const auto result = APDUParser::Parse(objects, handler, &logger);
            if (result != ParseResult::OK) {
                callback(GetFilesInfoTaskResult(TaskCompletion::FAILURE_BAD_RESPONSE));
                return ResponseResult::ERROR_BAD_RESPONSE;
            }

            fileTransportObject = handler.GetFileTransferObject();
            while (fileTransportObject.data.length() != 0)
            {
                Group70Var7 info;
                Group70Var7::Read(fileTransportObject.data, info);
                filesInfo.push_back(info.fileInfo);
            }

            currentTaskState = CLOSING;
            return ResponseResult::OK_REPEAT;
        }
        return ResponseResult::ERROR_BAD_RESPONSE;
    }

} // namespace opendnp3
