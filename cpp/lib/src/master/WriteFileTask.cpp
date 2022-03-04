#include "WriteFileTask.h"

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
    WriteFileTask::WriteFileTask(const std::shared_ptr<TaskContext>& context,
        IMasterApplication& app,
        const Logger& logger,
        std::shared_ptr<std::ifstream> source,
        std::string destFilename, uint16_t txSize,
        FileOperationTaskCallbackT taskCallback)
        : IMasterTask(context, app, TaskBehavior::SingleExecutionNoRetry(), logger, TaskConfig::Default()),
          input_file(std::move(source)), destFilename(std::move(destFilename)), _txSize(txSize)
    {
        callback = taskCallback ? std::move(taskCallback) : [](const FileOperationTaskResult& /**/) {};
        auto fsize = input_file->tellg();
        input_file->seekg(0, std::ios::end);
        fsize = input_file->tellg() - fsize;
        input_file->seekg(0);
        inputFileSize = static_cast<uint32_t>(fsize);
    }

    void WriteFileTask::Initialize()
    {
        fileCommandStatus = Group70Var4();
        fileTransportObject = Group70Var5(FileOpeningMode::WRITE);
    }

    bool WriteFileTask::BuildRequest(APDURequest& request, uint8_t seq) {
        switch (taskState) {
            case OPENING: {
                logger.log(flags::DBG, __FILE__, "Attempting opening file");
                Group70Var3 file(FileOpeningMode::WRITE);
                file.filename = destFilename;
                file.filesize = inputFileSize;
                file.blockSize = _txSize;
                request.SetFunction(FunctionCode::OPEN_FILE);
                request.SetControl(AppControlField::Request(seq));
                auto writer = request.GetWriter();
                return writer.WriteSingleValue<ser4cpp::UInt8, Group70Var3>(QualifierCode::FREE_FORMAT, file);
            }
            case WRITING: {
                fileTransportObject.fileId = fileCommandStatus.fileId;
                const uint32_t size = std::min(static_cast<uint32_t>(fileCommandStatus.blockSize), inputFileSize);
                fileTransportObject.data.make_empty();
                char* data = new char[size + 1];
                data[size] = 0;
                input_file->read(data, size);
                fileTransportObject.data = ser4cpp::rseq_t(reinterpret_cast<uint8_t const*>(data), size);
                inputFileSize -= size;
                if (inputFileSize == 0) {
                    fileTransportObject.isLastBlock = true;
                }
                request.SetFunction(FunctionCode::WRITE);
                request.SetControl(AppControlField::Request(seq));
                auto writer = request.GetWriter();
                const auto res = writer.WriteSingleValue<ser4cpp::UInt8, Group70Var5>(QualifierCode::FREE_FORMAT, fileTransportObject);
                delete[] data;
                return res;
            }
            case CLOSING: {
                request.SetFunction(FunctionCode::CLOSE_FILE);
                request.SetControl(AppControlField::Request(seq));
                auto writer = request.GetWriter();
                return writer.WriteSingleValue<ser4cpp::UInt8, Group70Var4>(QualifierCode::FREE_FORMAT, fileCommandStatus);
            }
            default:
                return false;
        }
    }

    IMasterTask::ResponseResult WriteFileTask::ProcessResponse(const APDUResponseHeader& response,
                                                              const ser4cpp::rseq_t& objects) {
        switch (taskState) {
            case OPENING:
            case CLOSING:
                return OnResponseStatusObject(response, objects);
            case WRITING:
                return OnResponseWriteFile(response, objects);
            default:
                return ResponseResult::ERROR_BAD_RESPONSE;
        }
    }

    IMasterTask::ResponseResult WriteFileTask::OnResponseStatusObject(const APDUResponseHeader& response,
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
            case FileCommandStatus::SUCCESS:
                if (taskState == OPENING) {
                    logger.log(flags::DBG, __FILE__, "Success opening file");
                    logger.log(flags::DBG, __FILE__, "Starting file writing...");
                    taskState = WRITING;
                    return ResponseResult::OK_REPEAT;
                }
                logger.log(flags::DBG, __FILE__, "Successfully closed file");
                if (errorWhileWriting) {
                    return ResponseResult::ERROR_BAD_RESPONSE;
                }
                return ResponseResult::OK_FINAL;
            case FileCommandStatus::PERMISSION_DENIED:
                logger.log(flags::DBG, __FILE__, "Permission denied");
                break;
            case FileCommandStatus::INVALID_MODE:
                logger.log(flags::DBG, __FILE__, "Invalid mode");
                break;
            case FileCommandStatus::NOT_FOUND:
                s = "File - \"" + destFilename + "\" not found";
                logger.log(flags::DBG, __FILE__, s.c_str());
                break;
            case FileCommandStatus::FILE_LOCKED:
                s = "File - \"" + destFilename + "\" locked by another user";
                logger.log(flags::DBG, __FILE__, s.c_str());
                break;
            case FileCommandStatus::OPEN_COUNT_EXCEEDED:
                logger.log(flags::DBG, __FILE__, "Maximum amount of files opened");
                break;
            case FileCommandStatus::FILE_NOT_OPEN:
                s = "File - \"" + destFilename + "\" not opened";
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

        return ResponseResult::ERROR_BAD_RESPONSE;
    }

    IMasterTask::ResponseResult WriteFileTask::OnResponseWriteFile(const APDUResponseHeader& header,
                                                                   const ser4cpp::rseq_t& objects) {
        if (ValidateSingleResponse(header)) {
            FileOperationHandler handler;
            const auto result = APDUParser::Parse(objects, handler, &logger);
            if (result != ParseResult::OK) {
                return ResponseResult::ERROR_BAD_RESPONSE;
            }

            fileTransportStatusObject = handler.GetFileTransferStatusObject();
            switch (fileTransportStatusObject.status) {
                case FileTransportStatus::SUCCESS:
                    if (fileTransportObject.isLastBlock) {
                        const std::string s = "File - \"" + destFilename +"\"  had been written successfully";
                        logger.log(flags::DBG, __FILE__, s.c_str());
                        taskState = CLOSING;
                        return ResponseResult::OK_REPEAT;
                    }

                    fileTransportObject.blockNumber = fileTransportStatusObject.blockNumber + 1;
                    return ResponseResult::OK_REPEAT;
                case FileTransportStatus::LOST_COM:
                    logger.log(flags::DBG, __FILE__, "Communication lost");
                    break;
                case FileTransportStatus::FILE_NOT_OPENED:
                    logger.log(flags::DBG, __FILE__, "Failed writing to the file which is not opened");
                    break;
                case FileTransportStatus::HANDLE_TIMEOUT:
                    logger.log(flags::DBG, __FILE__, "File handle expired, reopening file");
                    taskState = OPENING;
                    return ResponseResult::OK_REPEAT;
                case FileTransportStatus::BUFFER_OVERFLOW:
                    logger.log(flags::DBG, __FILE__, "Outstation buffer overflow while writing data");
                    break;
                case FileTransportStatus::FATAL_ERROR:
                    logger.log(flags::DBG, __FILE__, "Outstation fatal filesystem error");
                    break;
                case FileTransportStatus::OUT_OF_SEQUENCE:
                    logger.log(flags::DBG, __FILE__, "Data sequence is wrong");
                    break;
                default: 
                    logger.log(flags::DBG, __FILE__, "Unknown status code");
            }
        }

        errorWhileWriting = true;
        taskState = CLOSING;
        return ResponseResult::OK_REPEAT;
    }
} // namespace opendnp3
