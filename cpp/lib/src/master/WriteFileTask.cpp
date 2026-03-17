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
    WriteFileTask::WriteFileTask(
        const std::shared_ptr<TaskContext>& context,
        IMasterApplication& app,
        const Logger& logger,
        std::shared_ptr<std::ifstream> source,
        std::string destFilename,
        uint16_t txSize,
        const TaskBehavior& taskBehavior,
        FileOperationTaskCallbackT taskCallback
    )
        : IMasterTask(context, app, taskBehavior, logger, TaskConfig::Default())
        , _inputFile(std::move(source))
        , _destFilename(std::move(destFilename))
        , _txSize(txSize)
    {
        _callback = taskCallback ? std::move(taskCallback) : [](const FileOperationTaskResult& /**/) {};
        auto fsize = _inputFile->tellg();
        _inputFile->seekg(0, std::ios::end);
        fsize = _inputFile->tellg() - fsize;
        _inputFile->seekg(0);
        _inputFileSize = static_cast<uint32_t>(fsize);
    }

    void WriteFileTask::Initialize()
    {
        _fileCommandStatus = Group70Var4();
        _fileTransportObject = Group70Var5(FileOpeningMode::WRITE);
    }

    bool WriteFileTask::BuildRequest(APDURequest& request, uint8_t seq) {
        switch (_taskState) {
            case OPENING: {
                logger.log(flags::DBG, __FILE__, "Attempting opening file");
                Group70Var3 file(FileOpeningMode::WRITE);
                file.filename = _destFilename;
                file.filesize = _inputFileSize;
                file.blockSize = _txSize;
                request.SetFunction(FunctionCode::OPEN_FILE);
                request.SetControl(AppControlField::Request(seq));
                auto writer = request.GetWriter();
                return writer.WriteSingleValue<ser4cpp::UInt8, Group70Var3>(QualifierCode::FREE_FORMAT, file);
            }
            case WRITING: {
                _fileTransportObject.fileId = _fileCommandStatus.fileId;
                const uint32_t size = std::min(static_cast<uint32_t>(_fileCommandStatus.blockSize), _inputFileSize);
                _fileTransportObject.data.make_empty();
                char* data = new char[size + 1];
                data[size] = 0;
                _inputFile->read(data, size);
                _fileTransportObject.data = ser4cpp::rseq_t(reinterpret_cast<uint8_t const*>(data), size);
                _inputFileSize -= size;
                if (_inputFileSize == 0) {
                    _fileTransportObject.isLastBlock = true;
                }
                request.SetFunction(FunctionCode::WRITE);
                request.SetControl(AppControlField::Request(seq));
                auto writer = request.GetWriter();
                const auto res = writer.WriteSingleValue<ser4cpp::UInt8, Group70Var5>(QualifierCode::FREE_FORMAT, _fileTransportObject);
                delete[] data;
                return res;
            }
            case CLOSING: {
                request.SetFunction(FunctionCode::CLOSE_FILE);
                request.SetControl(AppControlField::Request(seq));
                auto writer = request.GetWriter();
                return writer.WriteSingleValue<ser4cpp::UInt8, Group70Var4>(QualifierCode::FREE_FORMAT, _fileCommandStatus);
            }
            default:
                return false;
        }
    }

    IMasterTask::ResponseResult WriteFileTask::ProcessResponse(const APDUResponseHeader& response,
                                                              const ser4cpp::rseq_t& objects) {
        switch (_taskState) {
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
            _fileCommandStatus = handler.GetFileStatusObject();
            std::string s;
            switch (_fileCommandStatus.status) {
            case FileCommandStatus::SUCCESS:
                if (_taskState == OPENING) {
                    logger.log(flags::DBG, __FILE__, "Success opening file");
                    logger.log(flags::DBG, __FILE__, "Starting file writing...");
                    _taskState = WRITING;
                    return ResponseResult::OK_REPEAT;
                }
                logger.log(flags::DBG, __FILE__, "Successfully closed file");
                if (_errorWhileWriting) {
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
                s = "File - \"" + _destFilename + "\" not found";
                logger.log(flags::DBG, __FILE__, s.c_str());
                break;
            case FileCommandStatus::FILE_LOCKED:
                s = "File - \"" + _destFilename + "\" locked by another user";
                logger.log(flags::DBG, __FILE__, s.c_str());
                break;
            case FileCommandStatus::OPEN_COUNT_EXCEEDED:
                logger.log(flags::DBG, __FILE__, "Maximum amount of files opened");
                break;
            case FileCommandStatus::FILE_NOT_OPEN:
                s = "File - \"" + _destFilename + "\" not opened";
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

            _fileTransportStatusObject = handler.GetFileTransferStatusObject();
            switch (_fileTransportStatusObject.status) {
                case FileTransportStatus::SUCCESS:
                    if (_fileTransportObject.isLastBlock) {
                        const std::string s = "File - \"" + _destFilename +"\"  had been written successfully";
                        logger.log(flags::DBG, __FILE__, s.c_str());
                        _taskState = CLOSING;
                        return ResponseResult::OK_REPEAT;
                    }

                    _fileTransportObject.blockNumber = _fileTransportStatusObject.blockNumber + 1;
                    return ResponseResult::OK_REPEAT;
                case FileTransportStatus::LOST_COM:
                    logger.log(flags::DBG, __FILE__, "Communication lost");
                    break;
                case FileTransportStatus::FILE_NOT_OPENED:
                    logger.log(flags::DBG, __FILE__, "Failed writing to the file which is not opened");
                    break;
                case FileTransportStatus::HANDLE_TIMEOUT:
                    logger.log(flags::DBG, __FILE__, "File handle expired, reopening file");
                    _taskState = OPENING;
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

        _errorWhileWriting = true;
        _taskState = CLOSING;
        return ResponseResult::OK_REPEAT;
    }
} // namespace opendnp3
