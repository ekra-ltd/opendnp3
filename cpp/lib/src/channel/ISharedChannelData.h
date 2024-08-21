#pragma once

#include "SharedTransmission.h"
#include "link/ILinkSession.h"
#include "opendnp3/link/Addresses.h"
#include "opendnp3/link/LinkHeaderFields.h"

#include <deque>
#include <memory>
#include <ser4cpp/container/SequenceTypes.h>

namespace opendnp3
{

    class ISharedChannelData
    {
    public:
        ISharedChannelData() = default;
        virtual ~ISharedChannelData() = default;

        ISharedChannelData(const ISharedChannelData& other) = delete;
        ISharedChannelData(ISharedChannelData&& other) noexcept = default;
        ISharedChannelData& operator=(const ISharedChannelData& other) = delete;
        ISharedChannelData& operator=(ISharedChannelData&& other) noexcept = default;

        virtual bool AddContext(const std::shared_ptr<ILinkSession>& session, const Addresses& addresses) = 0;
        virtual bool Remove(const std::shared_ptr<ILinkSession>& session) = 0;

        virtual bool Enable(const std::shared_ptr<ILinkSession>& session) = 0;
        virtual bool Disable(const std::shared_ptr<ILinkSession>& session) = 0;

        virtual bool SendToSession(const Addresses& addresses, const LinkHeaderFields& header, const ser4cpp::rseq_t& userdata) = 0;

        virtual void LowerLayerUp(LinkStateChangeSource source) = 0;

        virtual bool IsAnySessionEnabled() const = 0;

        virtual void LowerLayerDown(LinkStateChangeSource source) = 0;

        virtual std::deque<SharedTransmission>& TxQueue() = 0;
    };

} // namespace opendnp3
