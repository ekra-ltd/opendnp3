#include "opendnp3/channel/ChannelConnectionOptions.h"
#include <boost/variant/get.hpp>

#include <sstream>

namespace opendnp3
{
    namespace {
        enum class ConnectionType {
            Undefined,
            Serial,
            Tcp,
            Udp
        };

        struct CheckConnectionTypeVisitor
            : boost::static_visitor<ConnectionType>
        {
            ConnectionType operator()(const SerialSettings& /*unused*/) const noexcept {
                return ConnectionType::Serial;
            }
            ConnectionType operator()(const TCPSettings& /*unused*/) const noexcept {
                return ConnectionType::Tcp;
            }
            ConnectionType operator()(const UDPSettings& /*unused*/) const noexcept {
                return ConnectionType::Udp;
            }
            template<typename T>
            ConnectionType operator()(const T& /*unused*/) const noexcept {
                return ConnectionType::Undefined;
            }
        };

        CheckConnectionTypeVisitor DefaultCheckConnectionTypeVisitor;

        std::ostream& operator<<(std::ostream& os, const ConnectionType& arg) {
            switch (arg)
            {
                case ConnectionType::Serial:
                    return os << "Serial";
                case ConnectionType::Tcp:
                    return os << "Tcp";
                case ConnectionType::Udp:
                    return os << "Udp";
                case ConnectionType::Undefined:
                default:
                    return os << "<unknown>";
            }
        }
    }

    ChannelConnectionOptions::ChannelConnectionOptions(const SerialSettings& serialSettings)
        : _channelSettings(serialSettings)
    {}

    ChannelConnectionOptions::ChannelConnectionOptions(const TCPSettings& tcpSettings)
        : _channelSettings(tcpSettings)
    {}

    ChannelConnectionOptions::ChannelConnectionOptions(const UDPSettings& udpSettings)
        : _channelSettings(udpSettings)
    {}

    bool ChannelConnectionOptions::Enabled() const
    {
        return _enabled;
    }

    void ChannelConnectionOptions::Enabled(bool value)
    {
        _enabled = value;
    }

    bool ChannelConnectionOptions::UseSerial() const
    {
        return boost::apply_visitor(DefaultCheckConnectionTypeVisitor, _channelSettings) == ConnectionType::Serial;
    }

    bool ChannelConnectionOptions::UseTcp() const
    {
        return boost::apply_visitor(DefaultCheckConnectionTypeVisitor, _channelSettings) == ConnectionType::Tcp;
    }

    bool ChannelConnectionOptions::UseUdp() const
    {
        return boost::apply_visitor(DefaultCheckConnectionTypeVisitor, _channelSettings) == ConnectionType::Udp;
    }

    const SerialSettings& ChannelConnectionOptions::SerialPortParameters() const
    {
        return boost::get<SerialSettings>(_channelSettings);
    }

    const TCPSettings& ChannelConnectionOptions::TcpPortParameters() const
    {
        return boost::get<TCPSettings>(_channelSettings);
    }

    const UDPSettings& ChannelConnectionOptions::UDPParameters() const
    {
        return boost::get<UDPSettings>(_channelSettings);
    }

    bool ChannelConnectionOptions::IsBackupChannel() const
    {
        return _isBackupChannel;
    }

    void ChannelConnectionOptions::IsBackupChannel(const bool value)
    {
        _isBackupChannel = value;
    }

    unsigned ChannelConnectionOptions::ReadingCountBeforeReturnToPrimary() const
    {
        return _readingCountBeforeReturnToPrimary;
    }

    void ChannelConnectionOptions::ReadingCountBeforeReturnToPrimary(const unsigned value)
    {
        _readingCountBeforeReturnToPrimary = value;
    }

    std::string ChannelConnectionOptions::ToString() const
    {
        std::stringstream os;
        os << *this;
        return os.str();
    }

    bool operator==(const ChannelConnectionOptions& lhs, const ChannelConnectionOptions& rhs)
    {
        return lhs._name == rhs._name
            && lhs._enabled == rhs._enabled
            && lhs._channelSettings == rhs._channelSettings
            && lhs._isBackupChannel == rhs._isBackupChannel
            && lhs._readingCountBeforeReturnToPrimary == rhs._readingCountBeforeReturnToPrimary;
    }

    bool operator!=(const ChannelConnectionOptions& lhs, const ChannelConnectionOptions& rhs)
    {
        return !(lhs == rhs);
    }

    std::ostream& operator<<(std::ostream& os, const ChannelConnectionOptions& rhs)
    {
        const auto connectionType = boost::apply_visitor(DefaultCheckConnectionTypeVisitor, rhs._channelSettings);
        return os << "["
            << "ConnectionType: " << connectionType << " "
            << rhs._channelSettings
            << "]";
    }

} // namespace opendnp3
