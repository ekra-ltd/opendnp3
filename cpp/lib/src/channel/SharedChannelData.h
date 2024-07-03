#pragma once

#include "ISharedChannelData.h"
#include "IoSessionDescriptor.h"
#include "opendnp3/logging/Logger.h"

#include <vector>

namespace opendnp3
{

    class SharedChannelData final : public ISharedChannelData
    {
    public:
        explicit SharedChannelData(const Logger& logger);

        // ----- Implement ISharedChannelData -----

        bool AddContext(const std::shared_ptr<ILinkSession>& session, const Addresses& addresses) override;
        bool Remove(const std::shared_ptr<ILinkSession>& session) override;

        bool Enable(const std::shared_ptr<ILinkSession>& session) override;
        bool Disable(const std::shared_ptr<ILinkSession>& session) override;

        bool SendToSession(
            const Addresses& addresses,
            const LinkHeaderFields& header,
            const ser4cpp::rseq_t& userdata
        ) override;

        void LowerLayerUp() override;

        bool IsAnySessionEnabled() const override;

        void LowerLayerDown() override;

        std::deque<SharedTransmission>& TxQueue() override;

    private:
        bool isRouteInUse(const Addresses& addresses) const;
        bool isSessionInUse(const std::shared_ptr<ILinkSession>& session) const;

    private:
        std::vector<IoSessionDescriptor> _sessions;
        Logger _logger;
        std::deque<SharedTransmission> _txQueue;
    };

} // namespace opendnp3
