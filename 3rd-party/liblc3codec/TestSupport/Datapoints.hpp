/*
 * Datapoints.hpp
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

#ifndef __DATAPOINTS_HPP_
#define __DATAPOINTS_HPP_

#include <cstdint>

class Datapoint;
class ContainerRealization;

class DatapointContainer
{
    public:
        DatapointContainer();
        virtual ~DatapointContainer();

        void addDatapoint(const char* label, void* pData, uint16_t sizeInBytes);
        void addDatapoint(const char* label, const void* pData, uint16_t sizeInBytes);
        void log(const char* label, const void* pData, uint16_t sizeInBytes);

        uint16_t getDatapointSize(const char* label);
        bool getDatapointValue(const char* label, void* pDataBuffer, uint16_t bufferSize);
        bool setDatapointValue(const char* label, const void* pDataBuffer, uint16_t bufferSize);

    private:
        void addDatapoint(const char* label, Datapoint* pDatapoint);
        ContainerRealization* m_pContainer;
};

#endif // __DATAPOINTS_HPP_
