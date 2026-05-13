#pragma once

#include "app/AppControlField.h"
#include "app/HeaderWriter.h"
#include "app/parsing/FileOperationHandler.h"
#include "opendnp3/app/IINField.h"
#include "opendnp3/logging/Logger.h"

#include <boost/filesystem/fstream.hpp>
#include <boost/filesystem/operations.hpp>

#include <cstdint>
#include <fstream>
#include <map>
#include <string>
#include <utility>
#include <ser4cpp/container/Pair.h>

namespace opendnp3
{

namespace fs = boost::filesystem;

struct FileContext
{
    FileContext(std::string name, uint32_t size, bool dir,
            std::shared_ptr<fs::fstream> s, uint16_t blockSize, std::vector<uint8_t> di)
        : Name(std::move(name)), FileSize(size), IsDir(dir),
          Stream(std::move(s)), BlockSize(blockSize),
          BytesRemained(size), DirectoryInfo(std::move(di)) {}

    void Reset()
    {
        try
        {
            Stream->close();
        }
        catch (...) {}
        BytesRemained = FileSize;
        CurrentBlock = 0;
        DirectoryInfo.clear();
    }

    std::string Name;
    const uint32_t FileSize{0};
    bool IsDir;
    std::shared_ptr<fs::fstream> Stream;
    uint32_t BlockSize{ 0 };
    uint32_t CurrentBlock{ 0 };
    uint32_t BytesRemained{ 0 };
    std::vector<uint8_t> DirectoryInfo;
};

class FileTransferWorker
{
public:
    FileTransferWorker(bool enabled, uint32_t maxOpened, bool shouldOverride, bool permitDelete, Logger& logger);

    ser4cpp::Pair<IINField, AppControlField> HandleReadFile(const ser4cpp::rseq_t& objects, HeaderWriter* writer);

    IINField HandleOpenFile   (const ser4cpp::rseq_t& objects, HeaderWriter* writer);
    IINField HandleAuthFile   (const ser4cpp::rseq_t& objects, HeaderWriter* writer);
    IINField HandleAbortFile  (const ser4cpp::rseq_t& objects, HeaderWriter* writer);
    IINField HandleWriteFile  (const ser4cpp::rseq_t& objects, HeaderWriter* writer);
    IINField HandleCloseFile  (const ser4cpp::rseq_t& objects, HeaderWriter* writer);
    IINField HandleDeleteFile (const ser4cpp::rseq_t& objects, HeaderWriter* writer);
    IINField HandleGetFileInfo(const ser4cpp::rseq_t& objects, HeaderWriter* writer);

    void SetPrefferedBlockSize(uint32_t txSize, uint32_t rxSize);
    void Reset();

private:
    bool _enabled{ true };
    uint32_t _maxOpenedFiles{ 1 };
    bool _shouldOverride{ true };
    bool _permitDelete{ true };
    std::map<uint32_t, FileContext> _openedFiles;
    FileOperationHandler _fileOperationHandler;
    Logger _logger;
    uint32_t _openedFilesCount{ 0 };
    uint32_t _txSize{ 0 };
    uint32_t _rxSize{ 0 };
};

}
