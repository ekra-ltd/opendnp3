#pragma once

#include <opendnp3/link/Addresses.h>
#include <boost/optional/optional.hpp>
#include <unordered_map>
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

    using AddressesKey_t = boost::optional<Addresses>;

    using StatisticsChangeHandler_t = std::function<void(bool, StatisticsValueType, long long, AddressesKey_t)>;

    struct StatisticValueWithEvent
    {
        StatisticValueWithEvent(
            StatisticsValueType valueType = StatisticsValueType::None,
            StatisticsChangeHandler_t handler = nullptr
        )
            : ValueType(valueType)
            , ChangeHandler(std::move(handler))
        { }


        void Increment(bool isBackup, int64_t value, AddressesKey_t addresses = boost::none)
        {
            Values[boost::none] += value;
            if (addresses) {
                Values[addresses] += value;
            }
            if (ChangeHandler) {
                ChangeHandler(isBackup, ValueType, value, std::move(addresses));
            }
        }

        void Clear(const AddressesKey_t& addresses = boost::none)
        {
            if (!addresses) {
                Values.clear();
            }
            else {
                Values[addresses] = 0;
            }
        }

        StatisticValueWithEvent& operator=(const int64_t& other) = delete;

        std::unordered_map<AddressesKey_t, int64_t> Values;
        StatisticsValueType ValueType;
        StatisticsChangeHandler_t ChangeHandler;
    };
}
