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

    using StatisticsChangeHandler_t = std::function<void(bool, StatisticsValueType, long long)>;

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

        int64_t& operator++()
        {
            ++_value;
            if (_changeHandler) {
                _changeHandler(false, _valueType, 1);
            }
            return _value;
        }

        int64_t operator++(int /*v*/)
        {
            const int64_t old = _value;
            operator++();
            if (_changeHandler) {
                _changeHandler(false, _valueType, 1);
            }
            return old;
        }

        int64_t& operator+=(const int64_t& rhs)
        {
            _value += rhs;
            if (_changeHandler) {
                _changeHandler(false, _valueType, rhs);
            }
            return _value;
        }

        StatisticValueWithEvent& operator=(const int64_t& other)
        {
            _value = other;
            return *this;
        }

        StatisticsChangeHandler_t _changeHandler;
    };
}
