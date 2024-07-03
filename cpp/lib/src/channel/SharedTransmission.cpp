#include "SharedTransmission.h"

namespace opendnp3
{

    SharedTransmission::SharedTransmission(
        const ser4cpp::rseq_t& txdata,
        std::shared_ptr<ILinkSession> session
    )
        : TxData(txdata)
        , Session(std::move(session))
    {}

} // namespace opendnp3
