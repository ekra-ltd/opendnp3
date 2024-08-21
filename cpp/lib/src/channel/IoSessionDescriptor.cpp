#include "channel/IoSessionDescriptor.h"
#include "opendnp3/link/LinkStateChangeSource.h"

namespace opendnp3
{

    IoSessionDescriptor::IoSessionDescriptor(std::shared_ptr<ILinkSession> session, const Addresses& addresses)
        : _addresses(addresses)
        , _session(std::move(session))
    {}

    bool IoSessionDescriptor::Matches(const std::shared_ptr<ILinkSession>& session) const
    {
        return this->_session == session;
    }

    bool IoSessionDescriptor::Matches(const Addresses& addresses) const
    {
        return this->_addresses == addresses;
    }

    bool IoSessionDescriptor::OnFrame(const LinkHeaderFields& header, const ser4cpp::rseq_t& userdata) const
    {
        return this->_session->OnFrame(header, userdata);
    }

    bool IoSessionDescriptor::LowerLayerUp(LinkStateChangeSource source)
    {
        if (!_online)
        {
            _online = true;
            return this->_session->OnLowerLayerUp(source);
        }
        return false;
    }

    bool IoSessionDescriptor::LowerLayerDown(LinkStateChangeSource source)
    {
        if (_online)
        {
            _online = false;
            return this->_session->OnLowerLayerDown(source);
        }
        return false;
    }

    bool IoSessionDescriptor::Enabled() const
    {
        return _enabled;
    }

    void IoSessionDescriptor::Enabled(const bool value)
    {
        _enabled = value;
    }

} // namespace opendnp3
