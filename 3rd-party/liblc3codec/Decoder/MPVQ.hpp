/*
 * MPVQ.hpp
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

#ifndef __MPVQ_HPP_
#define __MPVQ_HPP_

#include <cstdint>

namespace Lc3Dec
{

void MPVQdeenum(uint8_t dim_in, /* i : dimension of vec_out */
                uint8_t k_val_in, /* i : number of unit pulses */
                int16_t LS_ind, /* i : leading sign index */
                int32_t MPVQ_ind, /* i : MPVQ shape index */
                int16_t *vec_out /* o : PVQ integer pulse train */
                );

}//namespace Lc3Dec

#endif // __MPVQ_HPP_
