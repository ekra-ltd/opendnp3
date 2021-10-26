#include "Group70.h"

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
        const auto length = strlen(arg.filename.c_str());
        const auto size = Size() + length;
        ser4cpp::UInt16::write_to(buffer, static_cast<uint16_t>(size)); // object size

        ser4cpp::UInt16::write_to(buffer, 26); // filename offset
        ser4cpp::UInt16::write_to(buffer, static_cast<uint16_t>(length)); // filename offset
        write_zeros(buffer, 6); // time of creation
        ser4cpp::UInt16::write_to(buffer, 292); // permissions - Access in read mode
        write_zeros(buffer, 4); // auth key
        write_zeros(buffer, 4); // file size
        ser4cpp::UInt16::write_to(buffer, 1); // operation mode - read 0x01
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
        const auto size = Size() + arg.dataSize;
        ser4cpp::UInt16::write_to(buffer, static_cast<uint16_t>(size));

        ser4cpp::UInt32::write_to(buffer, arg.fileId);
        ser4cpp::UInt32::write_to(buffer, arg.blockNumber);

        return true;
    }
}
