//
//  _   _         ______    _ _ _   _             _ _ _
// | \ | |       |  ____|  | (_) | (_)           | | | |
// |  \| | ___   | |__   __| |_| |_ _ _ __   __ _| | | |
// | . ` |/ _ \  |  __| / _` | | __| | '_ \ / _` | | | |
// | |\  | (_) | | |___| (_| | | |_| | | | | (_| |_|_|_|
// |_| \_|\___/  |______\__,_|_|\__|_|_| |_|\__, (_|_|_)
//                                           __/ |
//                                          |___/
// 
// This file is auto-generated. Do not edit manually
// 
// Copyright 2013-2022 Step Function I/O, LLC
// 
// Licensed to Green Energy Corp (www.greenenergycorp.com) and Step Function I/O
// LLC (https://stepfunc.io) under one or more contributor license agreements.
// See the NOTICE file distributed with this work for additional information
// regarding copyright ownership. Green Energy Corp and Step Function I/O LLC license
// this file to you under the Apache License, Version 2.0 (the "License"); you
// may not use this file except in compliance with the License. You may obtain
// a copy of the License at:
// 
//   http://www.apache.org/licenses/LICENSE-2.0
// 
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

#ifndef OPENDNP3_DOUBLEBIT_H
#define OPENDNP3_DOUBLEBIT_H

#include <cstdint>
#include <string>

namespace opendnp3 {

/**
  Enumeration for possible states of a double bit value
*/
enum class DoubleBit : uint8_t
{
  /// Transitioning between end conditions
  INTERMEDIATE = 0x0,
  /// End condition, determined to be OFF
  DETERMINED_OFF = 0x1,
  /// End condition, determined to be ON
  DETERMINED_ON = 0x2,
  /// Abnormal or custom condition
  INDETERMINATE = 0x3
};

struct DoubleBitSpec
{
  enum class BitPosition
  {
      ALL = -1,
      FIRST_BIT = 0,
      SECOND_BIT = 1
  };

  using enum_type_t = DoubleBit;

  static uint8_t to_type(DoubleBit arg);
  static DoubleBit from_type(uint8_t arg);
  static char const* to_string(DoubleBit arg);
  static char const* to_human_string(DoubleBit arg);
  static DoubleBit from_string(const std::string& arg);
};

inline DoubleBit operator&(DoubleBit a, DoubleBit b)
{
    return DoubleBitSpec::from_type(static_cast<uint8_t>(a) & static_cast<uint8_t>(b));
}

}

#endif
