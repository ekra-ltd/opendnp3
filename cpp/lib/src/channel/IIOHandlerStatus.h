#pragma once

namespace opendnp3
{

struct IIOHandlerStatus
{
    virtual ~IIOHandlerStatus() = default;

    virtual bool IsShutdown() const = 0;
};

} // namespace opendnp3
