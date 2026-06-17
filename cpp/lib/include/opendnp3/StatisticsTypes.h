#pragma once

#include <opendnp3/link/Addresses.h>
#include <boost/optional/optional.hpp>
#include <cstdint>
#include <functional>
#include <utility>

namespace opendnp3
{
    enum class StatisticsValueType {
        None,
        BytesSent,
        BytesReceived,
        FramesSent,
        FramesReceived,
        ConfirmationsSent,
        ConfirmationsReceived,
        ChecksumErrors,
        FrameFormatErrors,
        UnexpectedBytesReceived,
        SucceededConnections,
        FailedConnections,
        LostConnections,
        ConnectionState
    };

    enum class StatisticsConnectionStateType
    {
        Opened,
        Closed
    };

    using StatisticsChangeHandler_t = std::function<void(bool, StatisticsValueType, long long, boost::optional<Addresses>)>;

    struct StatisticValueWithEvent
    {
        StatisticValueWithEvent(
            int64_t value = 0,
            StatisticsValueType valueType = StatisticsValueType::None,
            StatisticsChangeHandler_t handler = nullptr
        )
            : _value(value)
            , _valueType(valueType)
            , _changeHandler(std::move(handler))
        { }

        int64_t _value;
        StatisticsValueType _valueType;

        void Increment(bool isBackup, int64_t value, boost::optional<Addresses> addresses = boost::none)
        {
            _value += value;
            if (_changeHandler) {
                _changeHandler(isBackup, _valueType, value, std::move(addresses));
            }
        }

        StatisticsChangeHandler_t _changeHandler;
    };
}
