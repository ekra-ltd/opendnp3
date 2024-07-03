#include "channel/SharedChannelData.h"

#include "logging/LogMacros.h"

#include <algorithm>

namespace opendnp3
{
    SharedChannelData::SharedChannelData(const Logger& logger)
        : _logger(logger)
    {}

    bool SharedChannelData::AddContext(const std::shared_ptr<ILinkSession>& session, const Addresses& addresses)
    {
        if (isRouteInUse(addresses))
        {
            FORMAT_LOG_BLOCK(_logger, flags::ERR, "Route already in use: %u -> %u", addresses.source, addresses.destination)
            return false;
        }

        if (isSessionInUse(session))
        {
            SIMPLE_LOG_BLOCK(_logger, flags::ERR, "Context cannot be bound 2x")
            return false;
        }

        _sessions.emplace_back(session, addresses); // record is always disabled by default

        return true;
    }

    bool SharedChannelData::Remove(const std::shared_ptr<ILinkSession>& session)
    {
        auto matches = [&](const IoSessionDescriptor& rec) { return rec.Matches(session); };
        const auto iter = std::find_if(_sessions.begin(), _sessions.end(), matches);

        if (iter == _sessions.end())
        {
            return false;
        }

        iter->LowerLayerDown();

        _sessions.erase(iter);
        return true;
    }

    bool SharedChannelData::Enable(const std::shared_ptr<ILinkSession>& session)
    {
        auto matches = [&](const IoSessionDescriptor& rec) { return rec.Matches(session); };

        const auto iter = std::find_if(_sessions.begin(), _sessions.end(), matches);

        if (iter == _sessions.end())
        {
            return false;
        }

        if (iter->Enabled())
        {
            return true; // already enabled
        }

        iter->Enabled(true);
        return true;
    }

    bool SharedChannelData::Disable(const std::shared_ptr<ILinkSession>& session)
    {
        auto matches = [&](const IoSessionDescriptor& rec) { return rec.Matches(session); };

        const auto iter = std::find_if(_sessions.begin(), _sessions.end(), matches);

        if (iter == _sessions.end())
        {
            return false;
        }

        if (!iter->Enabled())
        {
            return true; // already disabled
        }

        iter->Enabled(false);
        iter->LowerLayerDown();
        return true;
    }

    bool SharedChannelData::isRouteInUse(const Addresses& addresses) const
    {
        auto matches = [addresses](const IoSessionDescriptor& record) { return record.Matches(addresses); };
        return std::find_if(_sessions.begin(), _sessions.end(), matches) != _sessions.end();
    }

    bool SharedChannelData::SendToSession(
        const Addresses& /*addresses*/,
        const LinkHeaderFields& header,
        const ser4cpp::rseq_t& userdata
    ) {
        bool accepted = false;

        for (auto& session : _sessions)
        {
            if (session.Enabled())
            {
                accepted |= session.OnFrame(header, userdata);
            }
        }

        return accepted;
    }

    void SharedChannelData::LowerLayerUp()
    {
        for (auto& session : _sessions)
        {
            if (session.Enabled())
            {
                session.LowerLayerUp();
            }
        }
    }

    bool SharedChannelData::isSessionInUse(const std::shared_ptr<ILinkSession>& session) const
    {
        auto matches = [&](const IoSessionDescriptor& record) { return record.Matches(session); };
        return std::find_if(_sessions.begin(), _sessions.end(), matches) != _sessions.end();
    }

    bool SharedChannelData::IsAnySessionEnabled() const
    {
        auto matches = [&](const IoSessionDescriptor& record) { return record.Enabled(); };
        return std::find_if(_sessions.begin(), _sessions.end(), matches) != _sessions.end();
    }

    void SharedChannelData::LowerLayerDown()
    {
        for (auto& session : _sessions)
        {
            if (session.Enabled())
            {
                session.LowerLayerDown();
            }
        }
    }

    std::deque<SharedTransmission>& SharedChannelData::TxQueue()
    {
        return _txQueue;
    }

} // namespace opendnp3
