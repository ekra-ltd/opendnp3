#pragma once
#include <functional>
#include <cstdint>

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
        LostConnections
    };

    using StatisticsChangeHandler_t = std::function<void(StatisticsValueType, long long)>;

    struct StatisticValueWithEvent
    {
        StatisticValueWithEvent(uint64_t value = 0,
            StatisticsValueType valueType = StatisticsValueType::None,
            StatisticsChangeHandler_t handler = nullptr)
            : value(value),
            valueType(valueType),
            changeHandler(std::move(handler))
        { }

        uint64_t value;
        StatisticsValueType valueType;

        uint64_t& operator++()
        {
            ++value;
            if (changeHandler) {
                changeHandler(valueType, 1);
            }
            return value;
        }

        uint64_t operator++(int)
        {
            const uint64_t old = value;
            operator++();
            if (changeHandler) {
                changeHandler(valueType, 1);
            }
            return old;
        }

        uint64_t& operator+=(const uint64_t& rhs)
        {
            value += rhs;
            if (changeHandler) {
                changeHandler(valueType, rhs);
            }
            return value;
        }

        StatisticValueWithEvent& operator=(const uint64_t& other)
        {
            value = other;
            return *this;
        }

        StatisticsChangeHandler_t changeHandler;
    };
}
