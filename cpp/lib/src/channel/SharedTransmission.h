#pragma once

#include "link/ILinkSession.h"

#include <memory>
#include <ser4cpp/container/SequenceTypes.h>

namespace opendnp3
{

    struct SharedTransmission
    {
        SharedTransmission(const ser4cpp::rseq_t& txdata, std::shared_ptr<ILinkSession> session);

        SharedTransmission() = default;

        ser4cpp::rseq_t TxData;
        std::shared_ptr<ILinkSession> Session;
    };

} // namespace opendnp3
