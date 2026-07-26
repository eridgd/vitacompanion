#pragma once

#include <stdint.h>

#define VITACOMPANION_KERNEL_ABI_VERSION 1
#define VITACOMPANION_KERNEL_MODULE_NAME "vitacompanion_kernel"

int vitaCompanionKernelGetApiVersion(void);
int vitaCompanionKernelSetButtons(uint32_t buttons, int pressed);
int vitaCompanionKernelSetAnalog(int stick, int x, int y, int active);
int vitaCompanionKernelSetTouch(int port, int slot, int x, int y, int active);
int vitaCompanionKernelReset(void);
