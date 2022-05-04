//
//  _   _         ______    _ _ _   _             _ _ _
// | \ | |       |  ____|  | (_) | (_)           | | | |
// |  \| | ___   | |__   __| |_| |_ _ _ __   __ _| | | |
// | . ` |/ _ \  |  __| / _` | | __| | '_ \ / _` | | | |
// | |\  | (_) | | |___| (_| | | |_| | | | | (_| |_|_|_|
// |_| \_|\___/  |______\__,_|_|\__|_|_| |_|\__, (_|_|_)
//                                           __/ |
//                                          |___/
// 
// This file is auto-generated. Do not edit manually
// 
// Copyright 2013-2022 Step Function I/O, LLC
// 
// Licensed to Green Energy Corp (www.greenenergycorp.com) and Step Function I/O
// LLC (https://stepfunc.io) under one or more contributor license agreements.
// See the NOTICE file distributed with this work for additional information
// regarding copyright ownership. Green Energy Corp and Step Function I/O LLC license
// this file to you under the Apache License, Version 2.0 (the "License"); you
// may not use this file except in compliance with the License. You may obtain
// a copy of the License at:
// 
//   http://www.apache.org/licenses/LICENSE-2.0
// 
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

#ifndef OPENDNP3_GROUP70_H
#define OPENDNP3_GROUP70_H

#include "opendnp3/app/GroupVariationID.h"
#include "opendnp3/app/OctetData.h"
#include "opendnp3/master/DNPFileInfo.h"
#include "opendnp3/util/Buffer.h"

#include <ser4cpp/serialization/LittleEndian.h>

#include <string>
#include <vector>

namespace opendnp3 {

enum class FileCommandStatus : uint8_t {
    SUCCESS = 0x0,
    PERMISSION_DENIED = 0x1,
    INVALID_MODE = 0x2,
    NOT_FOUND = 0x3,
    FILE_LOCKED = 0x4,
    OPEN_COUNT_EXCEEDED = 0x5,
    FILE_NOT_OPEN = 0x6,
    INVALID_BLOCK_SIZE = 0x7,
    LOST_COM = 0x8,
    FAILED_ABORT = 0x9
};

enum class FileTransportStatus : uint8_t {
    SUCCESS = 0x0,
    LOST_COM = 0x8,
    FILE_NOT_OPENED = 0x10,
    HANDLE_TIMEOUT = 0x11,
    BUFFER_OVERFLOW = 0x12,
    FATAL_ERROR = 0x13,
    OUT_OF_SEQUENCE = 0x14
};

enum class FileOpeningMode : uint8_t
{
    DELETING = 0x0,
    READ = 0x1,
    WRITE = 0x2
};

// File-control - File identifier
struct Group70Var1 // obsolete
{
    static GroupVariationID ID() { return GroupVariationID(70,1); }
};

// File-control - Authentication
struct Group70Var2
{
    static GroupVariationID ID() { return GroupVariationID(70,2); }
};

// File-control - File command
struct Group70Var3
{
    /*     File Command Object
    +=====================+=======+
    |        Field        | Bytes |
    +=====================+=======+
    | File Name Offset    |   2   |
    +---------------------+-------+
    | File Name Size      |   2   |
    +---------------------+-------+
    | Time of creation    |   6   |
    +---------------------+-------+
    | Permissions         |   2   |
    +---------------------+-------+
    | Authentication Key  |   4   |
    +---------------------+-------+
    | File Size           |   4   |
    +---------------------+-------+
    | Operational Mode    |   2   |
    +---------------------+-------+
    | Maximum Block Size  |   2   |
    +---------------------+-------+
    | Request ID          |   2   |
    +---------------------+-------+
    | File Name           |   n   |
    +---------------------+-------+
    */

    Group70Var3(FileOpeningMode mode = FileOpeningMode::READ) : operationMode(mode) {}

    static GroupVariationID ID() { return GroupVariationID(70,3); }
    static size_t Size() { return 26; }
    static bool Read(ser4cpp::rseq_t& /*buffer*/, Group70Var3& /*arg*/);
    static bool Write(const Group70Var3& arg, ser4cpp::wseq_t& buffer);

    uint16_t objectSize{ 0 };
    FileOpeningMode operationMode{ FileOpeningMode::READ };
    std::string filename;
    uint32_t filesize = 0;
    uint16_t blockSize = 2048;
    uint16_t requestId{ 0 };
};

// File-control - File command status
// Used as a response to Open, Close, Delete and Abort commands (group 70 var 3)
struct Group70Var4
{
    /* File Command Status Object
    +=====================+=======+
    |        Name         | Bytes |
    +=====================+=======+
    | File Handle         |   4   |
    +---------------------+-------+
    | File Size           |   4   |
    +---------------------+-------+
    | Maximum Block Size  |   2   |
    +---------------------+-------+
    | Request ID          |   2   |
    +---------------------+-------+
    | Status              |   1   |
    +---------------------+-------+
    */

    Group70Var4() = default;
    Group70Var4(const FileCommandStatus s) : status(s) {  }

    static GroupVariationID ID() { return GroupVariationID(70,4); }
    static size_t Size() { return 13; }
    static bool Read(ser4cpp::rseq_t& buffer, Group70Var4& arg);
    static bool Write(const Group70Var4& arg, ser4cpp::wseq_t& buffer);

    uint16_t objectSize{ 0 };
    uint32_t fileId{ 0 };
    uint32_t fileSize{ 0 };
    uint16_t blockSize{ 0 };
    uint16_t requestId{ 0 };
    FileCommandStatus status{ FileCommandStatus::SUCCESS };
    bool isInitialize = false; // used to check GET_FILE_INFO func response
};

// File-control - File transport
struct Group70Var5
{
    /*
    +===============+=======+
    |     Name      | Bytes |
    +===============+=======+
    | File Handle   |   4   |
    +---------------+-------+
    | Block Number  |   4   |
    +---------------+-------+
    | Block Data    |   n   |
    +---------------+-------+
    */

    Group70Var5(FileOpeningMode mode = FileOpeningMode::READ) : operationMode(mode) {}

    static GroupVariationID ID() { return GroupVariationID(70,5); }
    static size_t Size() { return 8; }
    static bool Read(ser4cpp::rseq_t& buffer, Group70Var5& arg);
    static bool Write(const Group70Var5& /*arg*/, ser4cpp::wseq_t& /*buffer*/);

    uint16_t objectSize{ 0 };
    FileOpeningMode operationMode{ FileOpeningMode::READ };
    uint32_t fileId{ 0 };
    uint32_t blockNumber{ 0 };
    bool isLastBlock = false;
    ser4cpp::rseq_t data;
};

// File-control - File transport status
struct Group70Var6
{
    /*
    +===============+=======+
    |     Name      | Bytes |
    +===============+=======+
    | File Handle   |   4   |
    +---------------+-------+
    | Block Number  |   4   |
    +---------------+-------+
    | Status        |   1   |
    +---------------+-------+
    */

    static GroupVariationID ID() { return GroupVariationID(70,6); }
    static size_t Size() { return 9; }
    static bool Read(ser4cpp::rseq_t& buffer, Group70Var6& arg);
    static bool Write(const Group70Var6& arg, ser4cpp::wseq_t& buffer);

    uint16_t objectSize{ 0 };
    uint32_t fileId{ 0 };
    uint32_t blockNumber{ 0 };
    FileTransportStatus status{ FileTransportStatus::FILE_NOT_OPENED };
    std::string additionalInformation;
    bool isLastBlock = false;
};

// File-control - File descriptor
struct Group70Var7
{
    /*
    +===================+=======+
    |       Name        | Bytes |
    +===================+=======+
    | File Name Offset  |   2   |
    +-------------------+-------+
    | File Name Size    |   2   |
    +-------------------+-------+
    | File Type         |   2   |
    +-------------------+-------+
    | File Size         |   4   |
    +-------------------+-------+
    | Time of creation  |   6   |
    +-------------------+-------+
    | Permissions       |   2   |
    +-------------------+-------+
    | Request ID        |   2   |
    +-------------------+-------+
    | File Name         |   n   |
    +-------------------+-------+
    */

    static GroupVariationID ID() { return GroupVariationID(70,7); }
    static size_t Size() { return 20; }
    static bool Read(ser4cpp::rseq_t& buffer, Group70Var7& arg);
    static bool Write(const Group70Var7& arg, ser4cpp::wseq_t& buffer);

    uint16_t objectSize{ 0 };
    bool isInitialized{ false };
    DNPFileInfo fileInfo;
    uint16_t requestId{ 0 };
    bool asData = true; /// shows is this object part of the parent object or not
};

// File-control - File specification string
struct Group70Var8
{
    static GroupVariationID ID() { return GroupVariationID(70,8); }
};


}

#endif
