#include "DeleteFileTask.h"

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

    DeleteFileTask::DeleteFileTask(const std::shared_ptr<TaskContext>& context,
        IMasterApplication& app,
        const Logger& logger,
        std::string filename)
        : IMasterTask(context, app, TaskBehavior::SingleExecutionNoRetry(), logger, TaskConfig::Default()),
        filename(std::move(filename))
    { }

    void DeleteFileTask::Initialize()
    {
        fileCommandStatus = Group70Var4();
    }

    bool DeleteFileTask::BuildRequest(APDURequest& request, uint8_t seq) {
        logger.log(flags::DBG, __FILE__, "Attempting delete file");
        Group70Var3 file;
        file.filename = filename;
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
                return ResponseResult::ERROR_BAD_RESPONSE;
            }
            fileCommandStatus = handler.GetFileStatusObject();
            std::string s;
            switch (fileCommandStatus.status) {
            case FileCommandStatus::SUCCESS:
                s = "Success deleting file - \"" + filename + "\"";
                logger.log(flags::DBG, __FILE__, s.c_str());
                return ResponseResult::OK_FINAL;
            case FileCommandStatus::PERMISSION_DENIED:
                logger.log(flags::DBG, __FILE__, "Permission denied");
                break;
            case FileCommandStatus::INVALID_MODE:
                logger.log(flags::DBG, __FILE__, "Invalid mode");
                break;
            case FileCommandStatus::NOT_FOUND:
                s = "File - \"" + filename + "\" not found";
                logger.log(flags::DBG, __FILE__, s.c_str());
                break;
            case FileCommandStatus::FILE_LOCKED:
                s = "File - \"" + filename + "\" locked by another user";
                logger.log(flags::DBG, __FILE__, s.c_str());
                break;
            case FileCommandStatus::OPEN_COUNT_EXCEEDED:
                logger.log(flags::DBG, __FILE__, "Maximum amount of files opened");
                break;
            case FileCommandStatus::FILE_NOT_OPEN:
                s = "File - \"" + filename + "\" not opened";
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

} // namespace opendnp3
