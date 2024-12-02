#include "FileTransferWorker.h"

#include "IINHelpers.h"
#include "app/APDUHeader.h"
#include "app/parsing/APDUParser.h"
#include "gen/objects/Group70.h"
#include "util/EncodingConverter.h"

#include <boost/filesystem/directory.hpp>
#include <ser4cpp/container/Buffer.h>

#include <algorithm>
#include <vector>

namespace opendnp3
{

namespace
{
    uint32_t fileID = 0;
    uint32_t GetNextId()
    {
        return ++fileID;
    }

    IINField WriteFileCommandStatus(HeaderWriter* writer, Group70Var4 obj)
    {
        writer->WriteSingleValue<ser4cpp::UInt8, Group70Var4>(QualifierCode::FREE_FORMAT, obj);
        return IINField::Empty();
    }

    uint32_t DirectorySize(const fs::path& filePath)
    {
        uint32_t size = 0;
        for (const auto& f : fs::directory_iterator(filePath))
        {
            auto s = status(f);
            if (s.type() != boost::filesystem::regular_file && s.type() != boost::filesystem::directory_file)
            {
                continue;
            }
            // system dependend hidden file check
#ifdef BOOST_WINDOWS_API
            DWORD attributes = GetFileAttributes(f.path().string().c_str());
            if (attributes & FILE_ATTRIBUTE_HIDDEN)
            {
                continue;
            }
#endif
#ifdef BOOST_POSIX_API
            const auto pStr = f.path().filename().string();
            if (pStr == "." ||
                pStr == ".." ||
                pStr.rfind('.', 0) == 0)
            {
                continue;
            }
#endif
            size += 20; // header size
            size += f.path().filename().string().length(); // filename size
        }
        return size;
    }

    DNPFileOperationPermission ConvertPermissions(fs::perms p)
    {
        DNPFileOperationPermission result{DNPFileOperationPermission::NONE};
        if ((p & fs::owner_read) == fs::owner_read)
        {
            result |= DNPFileOperationPermission::OWNER_READ_ALLOWED;
        }
        if ((p & fs::owner_write) == fs::owner_write)
        {
            result |= DNPFileOperationPermission::OWNER_WRITE_ALLOWED;
        }
        if ((p & fs::owner_exe) == fs::owner_exe)
        {
            result |= DNPFileOperationPermission::OWNER_EXECUTE_ALLOWED;
        }
        if ((p & fs::group_read) == fs::group_read)
        {
            result |= DNPFileOperationPermission::GROUP_READ_ALLOWED;
        }
        if ((p & fs::group_write) == fs::group_write)
        {
            result |= DNPFileOperationPermission::GROUP_WRITE_ALLOWED;
        }
        if ((p & fs::group_exe) == fs::group_exe)
        {
            result |= DNPFileOperationPermission::GROUP_EXECUTE_ALLOWED;
        }
        if ((p & fs::others_read) == fs::others_read)
        {
            result |= DNPFileOperationPermission::WORLD_READ_ALLOWED;
        }
        if ((p & fs::others_write) == fs::others_write)
        {
            result |= DNPFileOperationPermission::WORLD_WRITE_ALLOWED;
        }
        if ((p & fs::others_exe) == fs::others_exe)
        {
            result |= DNPFileOperationPermission::WORLD_EXECUTE_ALLOWED;
        }
        return result;
    }

    std::string UnifyPath(std::string fileName)
    {
#ifdef BOOST_POSIX_API   // workaround
        std::replace(fileName.begin(), fileName.end(), '\\', '/');
#endif
        return fileName;
    }
}

FileTransferWorker::FileTransferWorker(bool enabled, uint32_t maxOpened, bool shouldOverride, bool permitDelete, Logger& logger)
: _enabled(enabled), _maxOpenedFiles(maxOpened), _shouldOverride(shouldOverride), _permitDelete(permitDelete), _logger(logger) {}

IINField FileTransferWorker::HandleOpenFile(const ser4cpp::rseq_t& objects, HeaderWriter* writer)
{
    if(!_enabled)
    {
        _logger.log(flags::DBG, __FILE__, "File transfer functions not supported");
        return { IINBit::FUNC_NOT_SUPPORTED };
    }

    Group70Var4 openResponce;
    openResponce.status = FileCommandStatus::FILE_NOT_OPEN;
    if (_openedFilesCount >= _maxOpenedFiles)
    {
        _logger.log(flags::DBG, __FILE__, "Reached the limit of opened files");
        openResponce.status = FileCommandStatus::OPEN_COUNT_EXCEEDED;
        return WriteFileCommandStatus(writer, openResponce);
    }

    _logger.log(flags::DBG, __FILE__, "Handling open file command...");
    const auto result = APDUParser::Parse(objects, _fileOperationHandler, _logger);
    if (result != ParseResult::OK)
    {
        _logger.log(flags::DBG, __FILE__, "Unsuccess while parsing");
        return IINFromParseResult(result);
    }

    _logger.log(flags::DBG, __FILE__, "Creating responce to open file command...");
    const auto openReq = _fileOperationHandler.GetFileCommandObject();

    std::string fName = UnifyPath(openReq.filename);
    const auto tFile = EncodingConverter::ToSystem(fName, CharacterSet::UTF8);
    const fs::path filePath = fs::path(tFile).make_preferred().native();
    auto s = "Working with the file/directory named = " + filePath.string();
    _logger.log(flags::DBG, __FILE__, s.c_str());
    const auto isDir = exists(filePath) && is_directory(filePath);
    if (openReq.operationMode == FileOpeningMode::READ)
    {
        openResponce.blockSize = std::min(openReq.blockSize, static_cast<uint16_t>(_txSize));
    }
    else
    {
        openResponce.blockSize = std::min(openReq.blockSize, static_cast<uint16_t>(_rxSize));
    }
    openResponce.requestId = openReq.requestId;

    const auto handler = std::find_if(_openedFiles.begin(), _openedFiles.end(), [&](const std::pair<uint32_t, FileContext>& p)
    {
        return p.second.Name == openReq.filename;
    });
    if (handler != _openedFiles.end())
    {
        openResponce.fileId = handler->first;
        openResponce.fileSize = handler->second.FileSize;
        openResponce.status = FileCommandStatus::FILE_LOCKED;
        return WriteFileCommandStatus(writer, openResponce);
    }

    auto file = std::make_shared<fs::fstream>();
    std::vector<uint8_t> dirInf;
    if (!isDir)
    {
        file->exceptions(std::fstream::badbit);
        if (openReq.operationMode != FileOpeningMode::WRITE)
        {
            if (!exists(filePath))
            {
                s = "File not found = " + filePath.string();
                _logger.log(flags::DBG, __FILE__, s.c_str());
                openResponce.status = FileCommandStatus::NOT_FOUND;
                return WriteFileCommandStatus(writer, openResponce);
            }
            const auto fileSize = file_size(filePath);
            openResponce.fileSize = static_cast<uint32_t>(fileSize);
            file->open(filePath, std::ios::in | std::ios::binary);
        }
        else
        {
            const auto parent = filePath.parent_path().make_preferred();
            if (!fs::exists(parent.native()))
            {
                boost::system::error_code ec;
                fs::create_directory(parent.native(), ec);
                if (ec)
                {
                    s = "Error creating directory = " + parent.string() + ec.message();
                    _logger.log(flags::DBG, __FILE__, s.c_str());
                    openResponce.status = FileCommandStatus::NOT_FOUND;
                    return WriteFileCommandStatus(writer, openResponce);
                }
            }
            if (_shouldOverride)
            {
                file->open(filePath, std::ios::out | std::ios::trunc | std::ios::binary);
            }
            else
            {
                file->open(filePath, std::ios::out | std::ios::app | std::ios::binary);
            }
        }
    }
    else
    {
        uint32_t size = 0;
        std::vector<Group70Var7> infos;
        for (const auto& f : fs::directory_iterator(filePath.string()))
        {
            auto path = f.path();
            path = path.make_preferred();
            const auto pStr = EncodingConverter::FromSystem(path.filename().string(), CharacterSet::UTF8);
            fs::file_status stat = status(path);
            if (stat.type() != boost::filesystem::regular_file && stat.type() != boost::filesystem::directory_file)
            {
                continue;
            }
            // system dependend hidden file check
#ifdef BOOST_WINDOWS_API
            DWORD attributes = GetFileAttributes(f.path().string().c_str());
            if (attributes & FILE_ATTRIBUTE_HIDDEN)
            {
                continue;
            }
#endif
#ifdef BOOST_POSIX_API
            if (pStr == "." ||
                pStr == ".." ||
                pStr.rfind('.', 0) == 0)
            {
                continue;
            }
#endif
            size += 20; // header size
            size += pStr.length(); // filename size

            Group70Var7 info;
            info.fileInfo.filename = pStr;
            info.fileInfo.fileSize = is_directory(path) ? DirectorySize(path) : static_cast<uint32_t>(file_size(path));
            info.fileInfo.fileType = is_directory(path) ? DNPFileType::DIRECTORY : DNPFileType::SIMPLE_FILE;
            info.fileInfo.permissions = ConvertPermissions(stat.permissions());
            info.isInitialized = true;
            info.asData = true;
            infos.push_back(info);
        }
        openResponce.fileSize = size;
        ser4cpp::Buffer b(size);
        ser4cpp::wseq_t buffer = b.as_wslice();
        ser4cpp::wseq_t b1(buffer);
        for (auto& info : infos)
        {
            Group70Var7::Write(info, b1);
        }
        const auto *data = static_cast<const uint8_t*>(buffer);
        dirInf = std::vector<uint8_t>(&data[0], &data[size]);
    }
    
    openResponce.status = FileCommandStatus::SUCCESS;
    const auto id = GetNextId();
    openResponce.fileId = id;
    _openedFiles.insert(std::make_pair(id, FileContext{ openReq.filename, openResponce.fileSize, isDir, file, openResponce.blockSize, dirInf }));
    ++_openedFilesCount;

    return WriteFileCommandStatus(writer, openResponce);
}

ser4cpp::Pair<IINField, AppControlField> FileTransferWorker::HandleReadFile(const ser4cpp::rseq_t& objects, HeaderWriter* writer)
{
    const auto control = AppControlField(true, true, false, false);
    if (!_enabled)
    {
        _logger.log(flags::DBG, __FILE__, "File transfer functions not supported");
        return ser4cpp::Pair<IINField, AppControlField>({ IINBit::FUNC_NOT_SUPPORTED }, control);
    }

    _logger.log(flags::DBG, __FILE__, "Handling read file command...");
    const auto result = APDUParser::Parse(objects, _fileOperationHandler, _logger);
    if (result != ParseResult::OK)
    {
        _logger.log(flags::DBG, __FILE__, "Unsuccess while parsing");
        return ser4cpp::Pair<IINField, AppControlField>(IINFromParseResult(result), control);
    }
    auto fileTransportObject = _fileOperationHandler.GetFileTransferObject();
    const auto handler = std::find_if(_openedFiles.begin(), _openedFiles.end(), [&](const std::pair<uint32_t, FileContext>& p)
    {
        return p.first == fileTransportObject.fileId;
    });
    Group70Var6 status;
    status.fileId = fileTransportObject.fileId;
    if (handler == _openedFiles.end())
    {
        _logger.log(flags::DBG, __FILE__, "File not opened before reading");
        status.blockNumber = 0;
        status.status = FileTransportStatus::FILE_NOT_OPENED;
        writer->WriteSingleValue<ser4cpp::UInt8, Group70Var6>(QualifierCode::FREE_FORMAT, status);
        return ser4cpp::Pair<IINField, AppControlField>(IINField::Empty(), control);
    }
    status.blockNumber = handler->second.CurrentBlock;
    if (fileTransportObject.blockNumber != handler->second.CurrentBlock)
    {
        _logger.log(flags::DBG, __FILE__, "Block number out of sequence");
        status.status = FileTransportStatus::OUT_OF_SEQUENCE;
        writer->WriteSingleValue<ser4cpp::UInt8, Group70Var6>(QualifierCode::FREE_FORMAT, status);
        return ser4cpp::Pair<IINField, AppControlField>(IINField::Empty(), control);
    }

    _logger.log(flags::DBG, __FILE__, "Sending data...");
    const uint32_t size = std::min(static_cast<uint32_t>(handler->second.BlockSize), handler->second.BytesRemained);
    fileTransportObject.data.make_empty();
    char* data = new char[size + 1];
    data[size] = 0;
    std::vector<uint8_t> r;
    if (handler->second.IsDir)
    {
        r.resize(size);
        std::copy_n(handler->second.DirectoryInfo.begin(), size, r.begin());
        fileTransportObject.data = ser4cpp::rseq_t(r.data(), size);
        handler->second.DirectoryInfo.erase(handler->second.DirectoryInfo.begin(), handler->second.DirectoryInfo.begin() + size);
    }
    else
    {
        handler->second.Stream->read(data, size);
        fileTransportObject.data = ser4cpp::rseq_t(reinterpret_cast<uint8_t const*>(data), size);
    }

    handler->second.BytesRemained -= size;
    if (handler->second.BytesRemained == 0) {
        fileTransportObject.isLastBlock = true;
    }
    ++handler->second.CurrentBlock;

    writer->WriteSingleValue<ser4cpp::UInt8, Group70Var5>(QualifierCode::FREE_FORMAT, fileTransportObject);
    delete[] data;
    return { IINField::Empty(), control };
}

IINField FileTransferWorker::HandleAuthFile(const ser4cpp::rseq_t& /*objects*/, HeaderWriter* /*writer*/)
{
    return { IINBit::FUNC_NOT_SUPPORTED };
}

IINField FileTransferWorker::HandleAbortFile(const ser4cpp::rseq_t& /*objects*/, HeaderWriter* writer)
{
    WriteFileCommandStatus(writer, Group70Var4(FileCommandStatus::FAILED_ABORT));
    return { IINBit::FUNC_NOT_SUPPORTED };
}

IINField FileTransferWorker::HandleWriteFile(const ser4cpp::rseq_t& objects, HeaderWriter* writer)
{
    if (!_enabled)
    {
        _logger.log(flags::DBG, __FILE__, "File transfer functions not supported");
        return { IINBit::FUNC_NOT_SUPPORTED };
    }

    _logger.log(flags::DBG, __FILE__, "Handling write file command...");
    const auto result = APDUParser::Parse(objects, _fileOperationHandler, _logger);
    if (result != ParseResult::OK)
    {
        _logger.log(flags::DBG, __FILE__, "Unsuccess while parsing");
        return IINFromParseResult(result);
    }
    auto fileTransportObject = _fileOperationHandler.GetFileTransferObject();
    const auto handler = std::find_if(_openedFiles.begin(), _openedFiles.end(), [&](const std::pair<uint32_t, FileContext>& p)
    {
        return p.first == fileTransportObject.fileId;
    });
    Group70Var6 status;
    status.fileId = fileTransportObject.fileId;
    if (handler == _openedFiles.end())
    {
        _logger.log(flags::DBG, __FILE__, "File not opened before writing");
        status.blockNumber = 0;
        status.status = FileTransportStatus::FILE_NOT_OPENED;
        writer->WriteSingleValue<ser4cpp::UInt8, Group70Var6>(QualifierCode::FREE_FORMAT, status);
        return IINField::Empty();
    }
    status.blockNumber = handler->second.CurrentBlock;
    if (fileTransportObject.blockNumber != handler->second.CurrentBlock)
    {
        _logger.log(flags::DBG, __FILE__, "Block number out of sequence");
        status.status = FileTransportStatus::OUT_OF_SEQUENCE;
        writer->WriteSingleValue<ser4cpp::UInt8, Group70Var6>(QualifierCode::FREE_FORMAT, status);
        return IINField::Empty();
    }
    const auto *data = static_cast<const uint8_t*>(fileTransportObject.data);
    handler->second.Stream->write(reinterpret_cast<const char *>(data), fileTransportObject.data.length());
    fileTransportObject.data.make_empty();
    if (fileTransportObject.isLastBlock) {
        const std::string s = "File - \"" + handler->second.Name + "\" successfully written";
        _logger.log(flags::DBG, __FILE__, s.c_str());
    }

    ++handler->second.CurrentBlock;
    status.status = FileTransportStatus::SUCCESS;
    writer->WriteSingleValue<ser4cpp::UInt8, Group70Var6>(QualifierCode::FREE_FORMAT, status);
    return IINField::Empty();
}

IINField FileTransferWorker::HandleCloseFile(const ser4cpp::rseq_t& objects, HeaderWriter* writer)
{
    if (!_enabled)
    {
        _logger.log(flags::DBG, __FILE__, "File transfer functions not supported");
        return { IINBit::FUNC_NOT_SUPPORTED };
    }

    _logger.log(flags::DBG, __FILE__, "Handling read file command...");
    const auto result = APDUParser::Parse(objects, _fileOperationHandler, _logger);
    if (result != ParseResult::OK)
    {
        _logger.log(flags::DBG, __FILE__, "Unsuccess while parsing");
        return IINFromParseResult(result);
    }
    auto statusObject = _fileOperationHandler.GetFileStatusObject();
    const auto handler = std::find_if(_openedFiles.begin(), _openedFiles.end(), [&](const std::pair<uint32_t, FileContext>& p)
    {
        return p.first == statusObject.fileId;
    });
    if (handler == _openedFiles.end())
    {
        statusObject.status = FileCommandStatus::FILE_NOT_OPEN;
        return WriteFileCommandStatus(writer, statusObject);
    }
    handler->second.Reset();
    _openedFiles.erase(handler->first);
    --_openedFilesCount;
    statusObject.status = FileCommandStatus::SUCCESS;
    return WriteFileCommandStatus(writer, statusObject);
}

IINField FileTransferWorker::HandleDeleteFile(const ser4cpp::rseq_t& objects, HeaderWriter* writer)
{
    if (!_enabled)
    {
        return { IINBit::FUNC_NOT_SUPPORTED };
    }
    if (!_permitDelete)
    {
        _logger.log(flags::DBG, __FILE__, "Deleting file is not permitted");
        return WriteFileCommandStatus(writer, Group70Var4(FileCommandStatus::PERMISSION_DENIED));
    }

    _logger.log(flags::DBG, __FILE__, "Handling delete file command...");
    const auto result = APDUParser::Parse(objects, _fileOperationHandler, _logger);
    if (result != ParseResult::OK)
    {
        _logger.log(flags::DBG, __FILE__, "Unsuccess while parsing");
        return IINFromParseResult(result);
    }
    const auto fileCommandObject = _fileOperationHandler.GetFileCommandObject();
    const auto handler = std::find_if(_openedFiles.begin(), _openedFiles.end(), [&](const std::pair<uint32_t, FileContext>& p)
    {
        return p.second.Name == fileCommandObject.filename;
    });
    Group70Var4 responce;
    if (handler != _openedFiles.end())
    {
        responce.status = FileCommandStatus::FILE_LOCKED;
        return WriteFileCommandStatus(writer, responce);
    }
    if (fileCommandObject.operationMode != FileOpeningMode::DELETING)
    {
        return { IINBit::PARAM_ERROR };
    }

    boost::system::error_code removeError;
    remove_all(fs::path(EncodingConverter::ToSystem(fileCommandObject.filename, CharacterSet::UTF8)), removeError);
    if (removeError)
    {
        if (removeError == boost::system::errc::errc_t::operation_not_permitted ||
            removeError == boost::system::errc::errc_t::permission_denied)
        {
            responce.status = FileCommandStatus::PERMISSION_DENIED;
        }
    }
    else
    {
        responce.status = FileCommandStatus::SUCCESS;
    }
    return WriteFileCommandStatus(writer, responce);
}

IINField FileTransferWorker::HandleGetFileInfo(const ser4cpp::rseq_t& objects, HeaderWriter* writer)
{
    if (!_enabled)
    {
        return { IINBit::FUNC_NOT_SUPPORTED };
    }

    const auto result = APDUParser::Parse(objects, _fileOperationHandler, _logger);
    if (result != ParseResult::OK)
    {
        _logger.log(flags::DBG, __FILE__, "Unsuccess while parsing");
        return IINFromParseResult(result);
    }
    auto fileStatus = _fileOperationHandler.GetFileDescriptorObject();
    fileStatus.asData = false;

    const fs::path filePath = fs::path(EncodingConverter::ToSystem(UnifyPath(fileStatus.fileInfo.filename), CharacterSet::UTF8)).make_preferred().native();
    if (!exists(filePath))
    {
        return WriteFileCommandStatus(writer, Group70Var4(FileCommandStatus::NOT_FOUND));
    }
    const auto& s = status(filePath);
    fileStatus.fileInfo.permissions = ConvertPermissions(s.permissions());
    if (is_directory(filePath))
    {
        fileStatus.fileInfo.fileType = DNPFileType::DIRECTORY;
        fileStatus.fileInfo.fileSize = DirectorySize(filePath);
    }
    else
    {
        fileStatus.fileInfo.fileType = DNPFileType::SIMPLE_FILE;
        fileStatus.fileInfo.fileSize = static_cast<uint32_t>(file_size(filePath));
    }
    fileStatus.isInitialized = true;

    writer->WriteSingleValue<ser4cpp::UInt8, Group70Var7>(QualifierCode::FREE_FORMAT, fileStatus);
    return IINField::Empty();
}

void FileTransferWorker::SetPrefferedBlockSize(uint32_t txSize, uint32_t rxSize)
{
    _rxSize = rxSize;
    _txSize = txSize;
}

void FileTransferWorker::Reset()
{
    try
    {
        for (auto& file : _openedFiles)
        {
            if (file.second.Stream && file.second.Stream->is_open())
            {
                file.second.Stream->close();
            }
        }
    }
    catch (...) {}
    _openedFiles.clear();
    _openedFilesCount = 0;
}

}
