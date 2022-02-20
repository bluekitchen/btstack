/*
 * BitReader.hpp
 *
 * Copyright 2019 HIMSA II K/S - www.himsa.com. Represented by EHIMA - www.ehima.com
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef __BIT_READER_HPP_
#define __BIT_READER_HPP_

#include <cstdint>

namespace Lc3Dec
{

uint8_t read_bit(const uint8_t bytes[], uint16_t* bp, uint8_t* mask);
uint16_t read_uint(const uint8_t bytes[], uint16_t* bp, uint8_t* mask, uint8_t numbits);

}//namespace Lc3Dec

#endif // __BIT_READER_HPP_
