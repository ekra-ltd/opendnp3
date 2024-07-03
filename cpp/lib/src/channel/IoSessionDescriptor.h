#pragma once

#include "link/ILinkSession.h"
#include <memory>

namespace opendnp3
{

	class IoSessionDescriptor
    {

    public:
        IoSessionDescriptor(std::shared_ptr<ILinkSession> session, const Addresses& addresses);

        IoSessionDescriptor() = default;

        bool Matches(const std::shared_ptr<ILinkSession>& session) const;

        bool Matches(const Addresses& addresses) const;

        bool OnFrame(const LinkHeaderFields& header, const ser4cpp::rseq_t& userdata) const;

        bool LowerLayerUp();

        bool LowerLayerDown();

        bool Enabled() const;
        void Enabled(bool value);

    private:
        bool _enabled{ false };
        Addresses _addresses;
        bool _online{ false };
        std::shared_ptr<ILinkSession> _session;
    };

} // namespace opendnp3
