/*
 * Copyright (c) 2020 Embedded Planet
 * SPDX-License-Identifier: Apache-2.0
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
 * limitations under the Licens
 *
 * Created on: Jul 30, 2020
 * Author: gdbeckstein
 */

#ifndef MCUBOOT_BOOT_MBED_INCLUDE_FLASH_MAP_BACKEND_SECONDARY_BD_H_
#define MCUBOOT_BOOT_MBED_INCLUDE_FLASH_MAP_BACKEND_SECONDARY_BD_H_

#include "blockdevice/BlockDevice.h"

/**
 * This is implemented as a weak function and may be redefined
 * by the application. The default case is to return the
 * BlockDevice object returned by BlockDevice::get_default_instance();
 *
 * @retval secondary_bd Secondary BlockDevice where update candidates are stored
 */
mbed::BlockDevice* get_secondary_bd(void);

#endif /* MCUBOOT_BOOT_MBED_INCLUDE_FLASH_MAP_BACKEND_SECONDARY_BD_H_ */
