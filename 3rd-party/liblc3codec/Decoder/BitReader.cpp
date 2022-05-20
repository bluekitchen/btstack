/*
 * BitReader.cpp
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

#include "BitReader.hpp"

namespace Lc3Dec
{

uint8_t read_bit(const uint8_t bytes[], uint16_t* bp, uint8_t* mask)
{
    uint8_t bit;
    if (bytes[*bp] & *mask)
    {
        bit = 1;
    }
    else
    {
        bit = 0;
    }
    if (*mask == 0x80)
    {
        *mask = 1;
        *bp -= 1;
    }
    else
    {
        *mask <<= 1;
    }
    return bit;
}

uint16_t read_uint(const uint8_t bytes[], uint16_t* bp, uint8_t* mask, uint8_t numbits)
{
    uint16_t value = read_bit(bytes, bp, mask);
    for (uint8_t i = 1; i < numbits; i++)
    {
        uint16_t bit = read_bit(bytes, bp, mask);
        value += bit << i;
    }
    return value;
}

}//namespace Lc3Dec
