#include "ReadFileTask.h"

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

    ReadFileTask::ReadFileTask(const std::shared_ptr<TaskContext>& context,
        IMasterApplication& app,
        const Logger& logger,
        std::string sourceFilename,
        const std::string& destFilename)
        : IMasterTask(context, app, TaskBehavior::SingleExecutionNoRetry(), logger, TaskConfig::Default()),
        sourceFilename(std::move(sourceFilename)),
        output_file(destFilename, std::ios::binary | std::ios::out)
    { }

    void ReadFileTask::Initialize()
    {
        fileCommandStatus = Group70Var4();
        fileTransportObject = Group70Var5();
    }

    void ReadFileTask::OnTaskComplete(TaskCompletion /*result*/, Timestamp /*now*/)
    {
        output_file.close();
    }

    bool ReadFileTask::BuildRequest(APDURequest& request, uint8_t seq) {
        switch (taskState) {
            case OPENING: {
                logger.log(flags::DBG, __FILE__, "Attempting opening file");
                Group70Var3 file;
                file.filename = sourceFilename;
                request.SetFunction(FunctionCode::OPEN_FILE);
                request.SetControl(AppControlField::Request(seq));
                auto writer = request.GetWriter();
                return writer.WriteSingleValue<ser4cpp::UInt8, Group70Var3>(QualifierCode::FREE_FORMAT, file);
            }
            case WORKING: {
                fileTransportObject.fileId = fileCommandStatus.fileId;
                request.SetFunction(FunctionCode::READ);
                request.SetControl(AppControlField::Request(seq));
                auto writer = request.GetWriter();
                return writer.WriteSingleValue<ser4cpp::UInt8, Group70Var5>(QualifierCode::FREE_FORMAT, fileTransportObject);
            }
            case CLOSING:
                request.SetFunction(FunctionCode::CLOSE_FILE);
                request.SetControl(AppControlField::Request(seq));
                auto writer = request.GetWriter();
                return writer.WriteSingleValue<ser4cpp::UInt8, Group70Var4>(QualifierCode::FREE_FORMAT, fileCommandStatus);
        }

        return false;
    }

    IMasterTask::ResponseResult ReadFileTask::ProcessResponse(const APDUResponseHeader& response,
                                                              const ser4cpp::rseq_t& objects) {
        switch (taskState) {
            case OPENING:
            case CLOSING:
                return OnResponseStatusObject(response, objects);
            case WORKING:
                return OnResponseReadFile(response, objects);
        }
        return ResponseResult::ERROR_BAD_RESPONSE;
    }

    IMasterTask::ResponseResult ReadFileTask::OnResponseStatusObject(const APDUResponseHeader& response,
                                                                 const ser4cpp::rseq_t& objects) {
        if (ValidateSingleResponse(response)) {
            FileOperationHandler handler;
            const auto result = APDUParser::Parse(objects, handler, &logger);
            if (result != ParseResult::OK) {
                return ResponseResult::ERROR_BAD_RESPONSE;
            }
            fileCommandStatus = handler.GetFileStatusObject();

            switch (fileCommandStatus.status) {
                case FileCommandStatus::SUCCESS:
                    if (taskState == OPENING) {
                        logger.log(flags::DBG, __FILE__, "Success opening file");
                        logger.log(flags::DBG, __FILE__, "Starting file reading...");
                        taskState = WORKING;
                        return ResponseResult::OK_REPEAT;
                    }
                    logger.log(flags::DBG, __FILE__, "Successfully closed file");
                    return ResponseResult::OK_FINAL;
                case FileCommandStatus::PERMISSION_DENIED:
                    logger.log(flags::DBG, __FILE__, "Permission denied");
                    break;
                case FileCommandStatus::INVALID_MODE:
                    logger.log(flags::DBG, __FILE__, "Invalid mode");
                    break;
                case FileCommandStatus::NOT_FOUND:
                    logger.log(flags::DBG, __FILE__, "File not found");
                    break;
                case FileCommandStatus::FILE_LOCKED:
                    logger.log(flags::DBG, __FILE__, "File locked by another user");
                    break;
                case FileCommandStatus::OPEN_COUNT_EXCEEDED:
                    logger.log(flags::DBG, __FILE__, "Maximum amount of files opened");
                    break;
                case FileCommandStatus::FILE_NOT_OPEN:
                    logger.log(flags::DBG, __FILE__, "Failed closing the file which is not opened");
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
                    return ResponseResult::ERROR_BAD_RESPONSE;
            }
        }

        return ResponseResult::ERROR_BAD_RESPONSE;
    }

    IMasterTask::ResponseResult ReadFileTask::OnResponseReadFile(const APDUResponseHeader& header,
                                                                 const ser4cpp::rseq_t& objects) {
        if (ValidateSingleResponse(header)) {
            FileOperationHandler handler;
            const auto result = APDUParser::Parse(objects, handler, &logger);
            if (result != ParseResult::OK) {
                return ResponseResult::ERROR_BAD_RESPONSE;
            }

            fileTransportObject = handler.GetFileTransferObject();
            const auto *data = static_cast<const uint8_t*>(fileTransportObject.data);
            output_file.write(reinterpret_cast<const char *>(data), fileTransportObject.data.length());
            fileTransportObject.data.make_empty();
            fileTransportObject.blockNumber += 1;
            if (fileTransportObject.isLastBlock) {
                taskState = CLOSING;
            }

            return ResponseResult::OK_REPEAT;
        }
        return ResponseResult::ERROR_BAD_RESPONSE;
    }
} // namespace opendnp3
