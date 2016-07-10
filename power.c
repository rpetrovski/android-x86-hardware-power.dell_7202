/*
 *
 * Copyright (C) 2016 The Android-x86 Open Source Project
 *
 * Licensed under the GNU General Public License Version 2 or later.
 * You may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.gnu.org/licenses/gpl.html
 *
 *
 */
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>

#include <unistd.h>
#include <sys/syscall.h>
#include <sys/types.h>

#define LOG_TAG "PowerHAL"
#include <utils/Log.h>

#include <hardware/hardware.h>
#include <hardware/power.h>

#define SYS_CPU "/sys/devices/system/cpu"
#define CPU_ONLINE "1"
#define CPU_OFFLINE "0"

static const char *hid_multitouch = "/system/lib/modules/4.4.12-android-x86/kernel/drivers/hid/hid-multitouch.ko";

static void set_nonboot_cpu_state(const char *state)
{
    static int cpu_n_fd;
    char p[PATH_MAX];
    int cpu_n = 1;

    for (;;) 
    {
        snprintf(p, PATH_MAX, SYS_CPU "/cpu%d/online", cpu_n);
        cpu_n_fd = open(p, O_RDWR);
        if (cpu_n_fd < 0) {
            return;
        }
        ALOGV("Set CPU%d_online state %s ", cpu_n, state);
        write(cpu_n_fd, state, 1);
        close(cpu_n_fd);
        cpu_n++;
    }
}

static void power_init(struct power_module *module)
{
}

static void load_hid_multitouch()
{
    // I've really tried to use system("modprobe hid_multitouch") and similar.
    // they don't work. So, this is how I load the module.

    struct stat s;
    const int fd = open(hid_multitouch, O_RDONLY);
    if (fd < 0) 
    {
        ALOGE("Unable to open module file for %s, errno: %d", hid_multitouch, errno);
        goto l_fd;
    }
    if (stat(hid_multitouch, &s))
    {
        ALOGE("stat failed for %s, errno: %d", hid_multitouch, errno);
        goto l_fd;
    }
    char *buffer = malloc(s.st_size);
    long res = read(fd, buffer, s.st_size);
    if (s.st_size != res)
    {
        ALOGE("read %lld bytes failed. Read only %ld bytes for %s, errno: %d", s.st_size, res, hid_multitouch, errno);
        goto l_buffer;
    }
    res = syscall(SYS_init_module, buffer, res, "");
    if (res) 
    {
        ALOGE("init_module failed for %s, errno: %d", hid_multitouch, errno);
        goto l_buffer;
    }
    ALOGI("loaded module %s", hid_multitouch);

l_buffer:
    free(buffer);
l_fd:
    close(fd);
}

static void power_set_interactive(struct power_module *module, int on)
{
    set_nonboot_cpu_state(on ? CPU_ONLINE : CPU_OFFLINE);

    if (on)
    {
        load_hid_multitouch();
    }
    else
    {
        if (syscall(SYS_delete_module, "hid_multitouch", 0))
        {
            ALOGE("delete_module failed for hid_multitouch. errno:%d", errno);
        }
        else
        {
            ALOGI("unloaded module %s", hid_multitouch);
        }
    }
}

static void power_hint(struct power_module *module, power_hint_t hint,
                       void *data) {
    switch (hint) {
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
        .name = "dell_7202 Power HAL",
        .author = "The Android-x86 Project",
        .methods = &power_module_methods,
    },

    .init = power_init,
    .setInteractive = power_set_interactive,
    .powerHint = power_hint,
};
