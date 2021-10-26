#include "Group70.h"

#include "opendnp3/app/DNPTime.h"



#include <chrono>
#include <ser4cpp/container/Buffer.h>

namespace opendnp3 {

    namespace {
        void write_zeros(ser4cpp::wseq_t& buffer, size_t amount) {
            for (size_t i = 0; i < amount; ++i) {
                buffer.put(0);
            }
        }
    }

    bool Group70Var3::Write(const Group70Var3& arg, ser4cpp::wseq_t& buffer)
    {
        const auto length = arg.filename.size();
        const auto size = Size() + length;
        ser4cpp::UInt16::write_to(buffer, static_cast<uint16_t>(size)); // object size
        ser4cpp::UInt16::write_to(buffer, static_cast<uint16_t>(Size())); // filename offset
        ser4cpp::UInt16::write_to(buffer, static_cast<uint16_t>(length)); // filename size
        if (arg.operationMode == FileOpeningMode::READ) {
            write_zeros(buffer, 6); // time of creation
        }
        else {
            std::chrono::milliseconds ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()
                );
            ser4cpp::UInt48::write_to(buffer, ser4cpp::UInt48Type(ms.count())); // time of creation
        }

        uint16_t perm;
        if (arg.operationMode == FileOpeningMode::READ) {
            perm = static_cast<uint16_t>(DNPFileOperationPermission::OWNER_READ_ALLOWED |
                                         DNPFileOperationPermission::GROUP_READ_ALLOWED |
                                         DNPFileOperationPermission::WORLD_READ_ALLOWED );
        }
        else {
            perm = static_cast<uint16_t>(DNPFileOperationPermission::OWNER_WRITE_ALLOWED |
                                         DNPFileOperationPermission::GROUP_WRITE_ALLOWED |
                                         DNPFileOperationPermission::WORLD_WRITE_ALLOWED );
        }
        ser4cpp::UInt16::write_to(buffer, perm); // permissions
        write_zeros(buffer, 4); // auth key
        ser4cpp::UInt32::write_to(buffer, arg.filesize); // file size
        ser4cpp::UInt16::write_to(buffer, static_cast<uint16_t>(arg.operationMode)); // operation mode - read/write
        ser4cpp::UInt16::write_to(buffer, 2048); // block size
        write_zeros(buffer, 2); // request id
        for (size_t i = 0; i < length; ++i) { // filename
            buffer.put(static_cast<uint8_t>(arg.filename[i]));
        }

        return true;
    }

    bool Group70Var4::Read(ser4cpp::rseq_t& buffer, Group70Var4& arg)
    {
        uint32_t fileHanlde = 0;
        if (ser4cpp::UInt32::read_from(buffer, fileHanlde)) {
            arg.fileId = fileHanlde;
        }
        uint32_t fileSize = 0;
        if (ser4cpp::UInt32::read_from(buffer, fileSize)) {
            arg.fileSize = fileSize;
        }
        uint16_t blockSize = 0;
        if (ser4cpp::UInt16::read_from(buffer, blockSize)) {
            arg.blockSize = blockSize;
        }
        uint16_t requestId = 0;
        if (ser4cpp::UInt16::read_from(buffer, requestId)) {
            arg.requestId = requestId;
        }
        uint8_t status = 0;
        if (ser4cpp::UInt8::read_from(buffer, status)) {
            arg.status = static_cast<FileCommandStatus>(status);
        }

        return true;
    }

    bool Group70Var4::Write(const Group70Var4& arg, ser4cpp::wseq_t& buffer)
    {
        ser4cpp::UInt16::write_to(buffer, static_cast<uint16_t>(Size())); // object size
        ser4cpp::UInt32::write_to(buffer, arg.fileId); // file handler
        ser4cpp::UInt32::write_to(buffer, arg.fileSize); // file size
        ser4cpp::UInt16::write_to(buffer, arg.blockSize); // block size
        ser4cpp::UInt16::write_to(buffer, arg.requestId); // request id
        ser4cpp::UInt8::write_to(buffer, 0); // status, should be 0

        return true;
    }

    bool Group70Var5::Read(ser4cpp::rseq_t& buffer, Group70Var5& arg)
    {
        uint32_t fileHanlde = 0;
        if (ser4cpp::UInt32::read_from(buffer, fileHanlde)) {
            arg.fileId = fileHanlde;
        }
        uint32_t blockNumber = 0;
        if (ser4cpp::UInt32::read_from(buffer, blockNumber)) {
            arg.isLastBlock = blockNumber >> 31 == 1;
            arg.blockNumber = blockNumber & 0x7FFFFFFF;
        }
        arg.data = buffer.take(arg.objectSize - Size());

        return true;
    }

    bool Group70Var5::Write(const Group70Var5& arg, ser4cpp::wseq_t& buffer)
    {
        const auto size = Size() + arg.data.length();
        ser4cpp::UInt16::write_to(buffer, static_cast<uint16_t>(size));
        ser4cpp::UInt32::write_to(buffer, arg.fileId);
        uint32_t blockNumber = arg.blockNumber;
        if (arg.isLastBlock) {
            blockNumber |= 0x80000000;
        }
        ser4cpp::UInt32::write_to(buffer, blockNumber);
        ser4cpp::rseq_t copy{ arg.data };
        while (copy.length() != 0)
        {
            uint8_t s = 0;
            ser4cpp::UInt8::read_from(copy, s);
            buffer.put(s);
        }

        return true;
    }

    bool Group70Var6::Read(ser4cpp::rseq_t& buffer, Group70Var6& arg)
    {
        uint32_t fileHanlde = 0;
        if (ser4cpp::UInt32::read_from(buffer, fileHanlde)) {
            arg.fileId = fileHanlde;
        }
        uint16_t blockSize = 0;
        if (ser4cpp::UInt16::read_from(buffer, blockSize)) {
            arg.blockNumber = blockSize;
        }
        uint8_t status = 0;
        if (ser4cpp::UInt8::read_from(buffer, status)) {
            arg.status = static_cast<FileTransportStatus>(status);
        }

        const auto* additionalData = static_cast<const uint8_t*>(buffer);
        arg.additionalInformation = std::string(reinterpret_cast<char const*>(additionalData), buffer.length());

        return true;
    }

    bool Group70Var7::Read(ser4cpp::rseq_t& buffer, Group70Var7& arg)
    {
        buffer.advance(2); // filename offset
        uint16_t filenameSize;
        ser4cpp::UInt16::read_from(buffer, filenameSize);
        uint16_t fileType;
        if (ser4cpp::UInt16::read_from(buffer, fileType)) {
            arg.fileInfo.fileType = static_cast<DNPFileType>(fileType);
        }
        uint32_t fileSize;
        if (ser4cpp::UInt32::read_from(buffer, fileSize)) {
            arg.fileInfo.fileSize = fileSize;
        }
        ser4cpp::UInt48Type ms;
        if (ser4cpp::UInt48::read_from(buffer, ms)) {
            arg.fileInfo.timeOfCreation = DNPTime(ms);
        }
        uint16_t permissions;
        if (ser4cpp::UInt16::read_from(buffer, permissions)) {
            arg.fileInfo.permissions = static_cast<DNPFileOperationPermission>(permissions);
        }
        uint16_t requestId;
        if (ser4cpp::UInt16::read_from(buffer, requestId)) {
            arg.requestId = requestId;
        }
        const auto* filename = static_cast<const uint8_t*>(buffer.take(filenameSize));
        buffer.advance(filenameSize);
        arg.fileInfo.filename = std::string(reinterpret_cast<char const*>(filename), filenameSize);
        arg.isInitialized = true;

        return true;
    }

    bool Group70Var7::Write(const Group70Var7& arg, ser4cpp::wseq_t& buffer)
    {
        const auto length = arg.fileInfo.filename.size();
        const auto size = Size() + length;
        ser4cpp::UInt16::write_to(buffer, static_cast<uint16_t>(size)); // object size
        ser4cpp::UInt16::write_to(buffer, static_cast<uint16_t>(Size())); // filename offset
        ser4cpp::UInt16::write_to(buffer, static_cast<uint16_t>(length)); // filename size
        write_zeros(buffer, 14);
        ser4cpp::UInt16::write_to(buffer, arg.requestId);
        for (size_t i = 0; i < length; ++i) { // filename
            buffer.put(static_cast<uint8_t>(arg.fileInfo.filename[i]));
        }

        return true;
    }
}
