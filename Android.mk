# Copyright (C) 2014 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

include external/stlport/libstlport.mk
LOCAL_C_INCLUDES += $(LOCAL_PATH)

LOCAL_MODULE := power.$(TARGET_BOARD_PLATFORM)
LOCAL_MODULE_PATH := $(TARGET_OUT_VENDOR_SHARED_LIBRARIES)/hw

# main libpower source
LOCAL_SRC_FILES := power.cpp

# for all devices under /sys/power/power_HAL_suspend
LOCAL_SRC_FILES += DevicePowerMonitor.cpp \
                   DevicePowerMonitorInfo.cpp \
                   CGroupCpusetController.cpp

LOCAL_SHARED_LIBRARIES := liblog libcutils libstlport

LOCAL_MODULE_TAGS := optional

LOCAL_MODULE_OWNER := intel

ifeq ($(POWERHAL_CHT), true)
   LOCAL_CFLAGS += -DPOWERHAL_CHT
endif

include $(BUILD_SHARED_LIBRARY)

include $(call first-makefiles-under,$(LOCAL_PATH))
