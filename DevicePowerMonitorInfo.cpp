/*
 * Copyright (C) 2014 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "DevicePowerMonitorInfo.h"

const char* DevicePowerMonitorInfo::deviceList[numDev] = {
    "i2c-ATML1000:00", /* ATMEL touchscreen */
    "i2c-GODX0911:00", /* Goodix GT911 touchscreen */
    "i2c-FT05506:00",  /* Focaltech ft5x0x touchscreen */
    "i2c-FTTH5506:00", /* Focaltech ft5x0x touchscreen */
    "i2c-MSFT0001:01"  /* Touchpad */
};
