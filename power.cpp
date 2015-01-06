/*
 * Copyright (C) 2014 Intel Corporation
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
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/un.h>

#include "include/hint.h"
#include <fcntl.h>

#define LOG_TAG "PowerHAL"
#include <utils/Log.h>

#include <cutils/log.h>
#include <hardware/hardware.h>
#include "CGroupCpusetController.h"
#include "DevicePowerMonitor.h"

#define SOCK_DEV "/dev/socket/power_hal"

#define ENABLE 1

static int sockfd;
static struct sockaddr_un client_addr;

static CGroupCpusetController cgroupCpusetController;
static DevicePowerMonitor powerMonitor;

#define TIMER_RATE_SYSFS	"/sys/devices/system/cpu/cpufreq/interactive/timer_rate"
#define BOOST_PULSE_SYSFS	"/sys/devices/system/cpu/cpufreq/interactive/boostpulse"
#define TOUCHBOOST_PULSE_SYSFS	"/sys/devices/system/cpu/cpufreq/interactive/touchboostpulse"
#define TOUCHBOOST_SYSFS	"/sys/devices/system/cpu/cpufreq/interactive/touchboost_freq"

/*
 * This parameter is to identify continuous touch/scroll events.
 * Any two touch hints received between a 20 interval ms is
 * considered as a scroll event.
 */
#define SHORT_TOUCH_TIME 20

/*
 * This parameter is to identify first touch events.
 * Any two touch hints received after 100 ms is considered as
 * a first touch event.
 */
#define LONG_TOUCH_TIME 100

/*
 * This parameter defines the number of vsync boost to be
 * done after the finger release event.
 */
#define VSYNC_BOOST_COUNT 4

/*
 * This parameter defines the time between a touch and a vsync
 * hint. the time if is > 30 ms, we do a vsync boost.
 */
#define VSYNC_TOUCH_TIME 30

int touchboost_disable = 0;
int timer_set = 0;
int vsync_boost = 0;

static void sysfs_write(char *path, char *s)
{
    char buf[80];
    int len;
    int fd = open(path, O_WRONLY);

    if (fd < 0) {
        strerror_r(errno, buf, sizeof(buf));
        ALOGE("Error opening %s: %s\n", path, buf);
        return;
    }

    len = write(fd, s, strlen(s));
    if (len < 0) {
        strerror_r(errno, buf, sizeof(buf));
        ALOGE("Error writing to %s: %s\n", path, buf);
    }

    close(fd);
}

static int sysfs_read(char *path, char *s, int num_bytes)
{
    char buf[80];
    int count;
    int ret = 0;
    int fd = open(path, O_RDONLY);
     if (fd < 0) {
        strerror_r(errno, buf, sizeof(buf));
        ALOGE("Error reading from %s: %s\n", path, buf);
        return -1;
    }
    if ((count = read(fd, s, (num_bytes - 1))) < 0) {
        strerror_r(errno, buf, sizeof(buf));
        ALOGE("Error reading from  %s: %s\n", path, buf);
        ret = -1;
    } else {
        s[count] = '\0';
    }
    close(fd);
    return ret;
}

static int socket_init()
{
    if (sockfd < 0) {
        sockfd = socket(PF_UNIX, SOCK_DGRAM, 0);
        if (sockfd < 0) {
            ALOGE("%s: failed to open: %s", __func__, strerror(errno));
            return 1;
        }
        memset(&client_addr, 0, sizeof(struct sockaddr_un));
        client_addr.sun_family = AF_UNIX;
        snprintf(client_addr.sun_path, UNIX_PATH_MAX, SOCK_DEV);
    }
    return 0;
}

static void power_init(__attribute__((unused))struct power_module *module)
{
    /* Enable all devices by default */
    powerMonitor.setState(ENABLE);
    cgroupCpusetController.setState(ENABLE);
    sockfd = -1;
    socket_init();

#if POWERHAL_CHT
    sysfs_write(TOUCHBOOST_SYSFS, "1360000");
#endif

}

static void power_set_interactive(__attribute__((unused))struct power_module *module, int on)
{
    powerMonitor.setState(on);
    cgroupCpusetController.setState(on);
}

static void power_hint_worker(power_hint_t hint, void *hint_data)
{
    int rc;
    power_hint_data_t data;
    if (socket_init()) {
        ALOGE("socket init failed");
        return;
    }

    data.hint = hint;

    if (NULL == hint_data) {
        data.data = 0;
    } else {
        data.data = 1;
    }

    rc = sendto(sockfd, &data, sizeof(data), 0, (const struct sockaddr *)&client_addr, sizeof(struct sockaddr_un));
    if (rc < 0) {
        ALOGE("%s: failed to send: %s", __func__, strerror(errno));
        return;
    }
}

static void power_hint(__attribute__((unused))struct power_module *module, power_hint_t hint,
                       void *data) {

    char sysfs_val[80];
    static struct timespec curr_time, prev_time = {0,0}, vsync_time;
    double diff;
    static int vsync_count;
    static int consecutive_touch_int;

    switch(hint) {
    case POWER_HINT_INTERACTION:
	clock_gettime(CLOCK_MONOTONIC, &curr_time);
	diff = (curr_time.tv_sec - prev_time.tv_sec) * 1000 +
		(double)(curr_time.tv_nsec - prev_time.tv_nsec) / 1e6;
	prev_time = curr_time;
	if(diff < SHORT_TOUCH_TIME)
		consecutive_touch_int ++;
	else if (diff > LONG_TOUCH_TIME) {
		vsync_boost = 0;
		timer_set = 0;
		touchboost_disable = 0;
		vsync_count = 0;
		consecutive_touch_int = 0;
		}
	/* Simple touch: timer rate need not be changed here */
	if((diff < SHORT_TOUCH_TIME) && (touchboost_disable == 0)
		&& (consecutive_touch_int > 4))
		touchboost_disable = 1;
	/*
	 * Scrolling: timer rate reduced to increase sensitivity. No more touch
	 * boost after this
	 */
	if((touchboost_disable == 1) && (consecutive_touch_int > 15)
		&& (timer_set == 0)) {
		timer_set = 1;
	}
	if (!touchboost_disable) {
		sysfs_write(TOUCHBOOST_PULSE_SYSFS,"1");
	}
	break;
    case POWER_HINT_VSYNC:
	if (touchboost_disable == 1) {
		clock_gettime(CLOCK_MONOTONIC, &vsync_time);
		diff = (vsync_time.tv_sec - curr_time.tv_sec) * 1000 +
			(double)(vsync_time.tv_nsec - curr_time.tv_nsec) / 1e6;
		if (diff > VSYNC_TOUCH_TIME) {
			timer_set = 0;
			vsync_boost = 1;
			touchboost_disable = 0;
			vsync_count = VSYNC_BOOST_COUNT;
		}
	}
	if (vsync_boost) {
		if (((int)data != 0) && (vsync_count > 0)) {
			sysfs_write(TOUCHBOOST_PULSE_SYSFS,"1");
			vsync_count--;
			if (vsync_count == 0)
				vsync_boost = 0;
                }
	}
	break;
    case POWER_HINT_LOW_POWER:
        power_hint_worker(POWER_HINT_LOW_POWER, data);
    default:
        break;
    }
}

static struct hw_module_methods_t power_module_methods = {
    .open = NULL,
};

struct power_module HAL_MODULE_INFO_SYM = {
    .common = {
        .tag = HARDWARE_MODULE_TAG,
        .module_api_version = POWER_MODULE_API_VERSION_0_2,
        .hal_api_version = HARDWARE_HAL_API_VERSION,
        .id = POWER_HARDWARE_MODULE_ID,
        .name = "Intel PC Compatible Power HAL",
        .author = "Intel Open Source Technology Center",
        .methods = &power_module_methods,
        .dso = NULL,
        .reserved = {},
    },

    .init = power_init,
    .setInteractive = power_set_interactive,
    .powerHint = power_hint,
};
