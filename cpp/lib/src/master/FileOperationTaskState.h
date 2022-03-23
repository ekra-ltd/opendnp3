#pragma once

namespace opendnp3
{

    enum FileOperationTaskState
    {
        OPENING,
        READING,
        WRITING,
        READING_DIRECTORY,
        CLOSING
    };

}