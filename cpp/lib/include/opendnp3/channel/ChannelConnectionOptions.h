#pragma once

#include "TLSConfig.h"
#include "UDPSettings.h"
#include "opendnp3/channel/SerialSettings.h"
#include "opendnp3/channel/TCPSettings.h"

#include <boost/variant/variant.hpp>

namespace opendnp3
{

class ChannelConnectionOptions
{
public:
    using ConnectionSettings_t = boost::variant<
        SerialSettings,
        TCPSettings,
        UDPSettings
    >;

public:
    ChannelConnectionOptions() = default;
    explicit ChannelConnectionOptions(const SerialSettings& serialSettings);
    explicit ChannelConnectionOptions(const TCPSettings& tcpSettings);
    explicit ChannelConnectionOptions(const UDPSettings& udpSettings);

    bool Enabled() const;
    void Enabled(bool value);

    bool UseSerial() const;
    bool UseTcp() const;
    bool UseUdp() const;

    const SerialSettings& SerialPortParameters() const;

    const TCPSettings& TcpPortParameters() const;

    const UDPSettings& UDPParameters() const;

    bool IsBackupChannel() const;
    void IsBackupChannel(bool value);

    unsigned ReadingCountBeforeReturnToPrimary() const;
    void ReadingCountBeforeReturnToPrimary(unsigned value);

    std::string ToString() const;

    friend bool operator==(const ChannelConnectionOptions& lhs, const ChannelConnectionOptions& rhs);
    friend bool operator!=(const ChannelConnectionOptions& lhs, const ChannelConnectionOptions& rhs);
    friend std::ostream& operator <<(std::ostream& os, const ChannelConnectionOptions& rhs);

private:
    std::string _name;
    bool _enabled{ true };
    ConnectionSettings_t _channelSettings{
        SerialSettings{}
    };
    bool _isBackupChannel{ false };
    unsigned _readingCountBeforeReturnToPrimary{ 0 };
};

} // namespace opendnp3

