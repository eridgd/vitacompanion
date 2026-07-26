#include "version.h"

#include <psp2/kernel/modulemgr.h>
#include <stdio.h>
#include <string.h>

int version_format(char *buffer, size_t buffer_size)
{
    SceKernelModuleInfo info;
    SceUID module_id;

    if (!buffer || buffer_size == 0)
        return -1;

    memset(&info, 0, sizeof(info));
    info.size = sizeof(info);

    module_id = sceKernelGetModuleIdByAddr((void *)&version_format);
    if (module_id < 0 || sceKernelGetModuleInfo(module_id, &info) < 0)
        return -1;

    return snprintf(buffer, buffer_size, "vitacompanion %u.%02u\n",
        (unsigned int)info.modver[1], (unsigned int)info.modver[0]);
}
