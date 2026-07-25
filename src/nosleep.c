#include "nosleep.h"

#include "log.h"

#include <psp2/kernel/processmgr.h>
#include <psp2/kernel/threadmgr.h>
#include <stdbool.h>

extern volatile int run;

static SceUID nosleep_thid = -1;
static SceUID nosleep_mtx = -1;
static volatile bool nosleep_enabled = true;
static volatile bool nosleep_locked = false;

static void nosleep_lock(void)
{
    if (nosleep_mtx >= 0)
        sceKernelLockMutex(nosleep_mtx, 1, NULL);

    if (!nosleep_locked)
    {
        int ret = sceKernelPowerLock(SCE_KERNEL_POWER_TICK_DISABLE_AUTO_SUSPEND);
        LOG("sceKernelPowerLock: 0x%08X\n", ret);

        if (ret >= 0)
            nosleep_locked = true;
    }

    if (nosleep_mtx >= 0)
        sceKernelUnlockMutex(nosleep_mtx, 1);
}

static void nosleep_unlock(void)
{
    if (nosleep_mtx >= 0)
        sceKernelLockMutex(nosleep_mtx, 1, NULL);

    if (nosleep_locked)
    {
        int ret = sceKernelPowerUnlock(SCE_KERNEL_POWER_TICK_DISABLE_AUTO_SUSPEND);
        LOG("sceKernelPowerUnlock: 0x%08X\n", ret);

        if (ret >= 0)
            nosleep_locked = false;
    }

    if (nosleep_mtx >= 0)
        sceKernelUnlockMutex(nosleep_mtx, 1);
}

static int nosleep_thread(unsigned int args, void *argp)
{
    (void)args;
    (void)argp;

    while (run)
    {
        if (nosleep_enabled)
        {
            nosleep_lock();
            sceKernelPowerTick(SCE_KERNEL_POWER_TICK_DISABLE_AUTO_SUSPEND);
        }
        else
        {
            nosleep_unlock();
        }

        sceKernelDelayThread(1000 * 1000);
    }

    nosleep_unlock();
    sceKernelExitDeleteThread(0);
    return 0;
}

int nosleep_start(void)
{
    int result;

    if (nosleep_thid >= 0)
        return -1;

    nosleep_enabled = true;
    nosleep_mtx = sceKernelCreateMutex(
        "vitacompanion_nosleep_mutex", 0, 0, NULL);
    if (nosleep_mtx < 0)
        return nosleep_mtx;

    nosleep_thid = sceKernelCreateThread("vitacompanion_nosleep_thread", nosleep_thread, 0x40, 0x10000, 0, 0, NULL);
    if (nosleep_thid < 0)
    {
        result = nosleep_thid;
        sceKernelDeleteMutex(nosleep_mtx);
        nosleep_mtx = -1;
        return result;
    }

    result = sceKernelStartThread(nosleep_thid, 0, NULL);
    if (result < 0)
    {
        sceKernelDeleteThread(nosleep_thid);
        sceKernelDeleteMutex(nosleep_mtx);
        nosleep_thid = -1;
        nosleep_mtx = -1;
        return result;
    }

    return 0;
}

void nosleep_end(void)
{
    nosleep_set_enabled(false);

    if (nosleep_thid >= 0)
    {
        sceKernelWaitThreadEnd(nosleep_thid, NULL, NULL);
        nosleep_thid = -1;
    }

    if (nosleep_mtx >= 0)
    {
        sceKernelDeleteMutex(nosleep_mtx);
        nosleep_mtx = -1;
    }
}

void nosleep_set_enabled(bool enabled)
{
    nosleep_enabled = enabled;

    if (enabled)
    {
        nosleep_lock();
        sceKernelPowerTick(SCE_KERNEL_POWER_TICK_DISABLE_AUTO_SUSPEND);
    }
    else
    {
        nosleep_unlock();
    }
}

bool nosleep_is_enabled(void)
{
    return nosleep_enabled;
}
