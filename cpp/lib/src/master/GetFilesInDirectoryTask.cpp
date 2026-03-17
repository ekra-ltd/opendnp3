#include "GetFilesInDirectoryTask.h"

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

    GetFilesInDirectoryTask::GetFilesInDirectoryTask(
        const std::shared_ptr<TaskContext>& context,
        IMasterApplication& app,
        const Logger& logger,
        std::string sourceDirectory,
        const TaskBehavior& taskBehavior,
        GetFilesInfoTaskCallbackT taskCallback,
        uint16_t rxSize
    )
        : IMasterTask(context, app, taskBehavior, logger, TaskConfig::Default())
        , _sourceDirectory(std::move(sourceDirectory))
        , _callback(std::move(taskCallback))
        , _rxSize(rxSize)
    { }

    void GetFilesInDirectoryTask::Initialize()
    {
        _currentTaskState = OPENING;
        _fileCommandStatus = Group70Var4();
        _fileTransportObject = Group70Var5();
        _filesInfo.clear();
        if (_callback == nullptr) {
            _callback = [](const GetFilesInfoTaskResult& /**/) {};
        }
    }

    bool GetFilesInDirectoryTask::BuildRequest(APDURequest& request, uint8_t seq) {
        switch (_currentTaskState) {
            case OPENING: {
                logger.log(flags::DBG, __FILE__, "Attempting opening directory");
                Group70Var3 file;
                file.filename = _sourceDirectory;
                file.blockSize = _rxSize;
                request.SetFunction(FunctionCode::OPEN_FILE);
                request.SetControl(AppControlField::Request(seq));
                auto writer = request.GetWriter();
                return writer.WriteSingleValue<ser4cpp::UInt8, Group70Var3>(QualifierCode::FREE_FORMAT, file);
            }
            case READING_DIRECTORY: {
                _fileTransportObject.fileId = _fileCommandStatus.fileId;
                request.SetFunction(FunctionCode::READ);
                request.SetControl(AppControlField::Request(seq));
                auto writer = request.GetWriter();
                return writer.WriteSingleValue<ser4cpp::UInt8, Group70Var5>(QualifierCode::FREE_FORMAT, _fileTransportObject);
            }
            case CLOSING: {
                request.SetFunction(FunctionCode::CLOSE_FILE);
                request.SetControl(AppControlField::Request(seq));
                auto writer = request.GetWriter();
                return writer.WriteSingleValue<ser4cpp::UInt8, Group70Var4>(QualifierCode::FREE_FORMAT, _fileCommandStatus);
            }
        }

        return false;
    }

    IMasterTask::ResponseResult GetFilesInDirectoryTask::ProcessResponse(const APDUResponseHeader& response,
                                                              const ser4cpp::rseq_t& objects) {
        switch (_currentTaskState) {
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
            _fileCommandStatus = handler.GetFileStatusObject();
            std::string s;
            switch (_fileCommandStatus.status) {
                case FileCommandStatus::SUCCESS: {
                    switch (_currentTaskState) {
                        case OPENING:
                            s = "Reading files in directory - \"" + _sourceDirectory + "\"";
                            logger.log(flags::DBG, __FILE__, s.c_str());
                            _currentTaskState = READING_DIRECTORY;
                            return ResponseResult::OK_REPEAT;
                        case READING_DIRECTORY:
                            logger.log(flags::DBG, __FILE__, "Successfully received file names");
                            logger.log(flags::DBG, __FILE__, "Reading each file...");
                            _currentTaskState = CLOSING;
                            return ResponseResult::OK_REPEAT;
                        case CLOSING:
                            s = "Successfully closed directory - \"" + _sourceDirectory + "\"";
                            logger.log(flags::DBG, __FILE__, s.c_str());
                            if (_errorWhileReading) {
                                _callback(GetFilesInfoTaskResult(TaskCompletion::FAILURE_BAD_RESPONSE));
                                return ResponseResult::ERROR_BAD_RESPONSE;
                            }
                            _callback(GetFilesInfoTaskResult(TaskCompletion::SUCCESS, _filesInfo));
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
                    s = "Directory - \"" + _sourceDirectory + "\" not found";
                    logger.log(flags::DBG, __FILE__, s.c_str());
                    break;
                case FileCommandStatus::FILE_LOCKED:
                    s = "Directory - \"" + _sourceDirectory + "\" locked by another user";
                    logger.log(flags::DBG, __FILE__, s.c_str());
                    break;
                case FileCommandStatus::OPEN_COUNT_EXCEEDED:
                    logger.log(flags::DBG, __FILE__, "Maximum amount of files opened");
                    break;
                case FileCommandStatus::FILE_NOT_OPEN:
                    s = "Directory - \"" + _sourceDirectory + "\" not opened";
                    logger.log(flags::DBG, __FILE__, s.c_str());
                    break;
                case FileCommandStatus::INVALID_BLOCK_SIZE:
                    logger.log(flags::DBG, __FILE__, "Cannot write this block size");
                    break;
                case FileCommandStatus::LOST_COM:
                    logger.log(flags::DBG, __FILE__, "Communication lost");
                    _callback(GetFilesInfoTaskResult(TaskCompletion::FAILURE_NO_COMMS));
                    break;
                case FileCommandStatus::FAILED_ABORT:
                    logger.log(flags::DBG, __FILE__, "Abort action failed");
                    break;
                default:
                    logger.log(flags::DBG, __FILE__, "Unknown status code");
                    break;
            }
        }

        _callback(GetFilesInfoTaskResult(TaskCompletion::FAILURE_BAD_RESPONSE));
        return ResponseResult::ERROR_BAD_RESPONSE;
    }

    IMasterTask::ResponseResult GetFilesInDirectoryTask::OnResponseReadDirectory(const APDUResponseHeader& header,
                                                                           const ser4cpp::rseq_t& objects) {
        if (ValidateSingleResponse(header)) {
            FileOperationHandler handler;
            const auto result = APDUParser::Parse(objects, handler, &logger);
            if (result != ParseResult::OK) {
                _errorWhileReading = true;
                _currentTaskState = CLOSING;
                return ResponseResult::OK_REPEAT;
            }

            _fileTransportObject = handler.GetFileTransferObject();
            while (_fileTransportObject.data.length() != 0)
            {
                Group70Var7 info;
                Group70Var7::Read(_fileTransportObject.data, info);
                _filesInfo.push_back(info.fileInfo);
            }

            _currentTaskState = CLOSING;
            return ResponseResult::OK_REPEAT;
        }
        return ResponseResult::ERROR_BAD_RESPONSE;
    }

} // namespace opendnp3
